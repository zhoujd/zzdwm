#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ai.c"

int
main (void)
{
  char output[4096];
  char *request = NULL;
  char large_prompt[2048];
  size_t length = 0;

  assert (ai_configure ("http://127.0.0.1:11434/api/generate", NULL)
          == TRUE);
  assert (strcmp (ai_host, "127.0.0.1") == 0);
  assert (ai_port == 11434);
  assert (strcmp (ai_path, "/api/generate") == 0);
  assert (ai_api_key[0] == 0);

  assert (ai_build_request ("He said \"hello\"\n", "test-model",
                            3, &request, &length) == TRUE);
  assert (strstr (request, "POST /api/generate HTTP/1.0") != NULL);
  assert (strstr (request, "Host: 127.0.0.1:11434") != NULL);
  assert (strstr (request, "\"test-model\"") != NULL);
  assert (strstr (request, "{\"model\":\"test-model\"") != NULL);
  assert (strstr (request,
                  "\"system\":\"You are a helpful assistant inside MicroEMACS.")
          != NULL);
  assert (strstr (request, "\"prompt\":\"He said \\\"hello\\\"\\n\"")
          != NULL);
  assert (strstr (request, "\"stream\":false") != NULL);
  assert (strstr (request, "\"num_predict\":3}") != NULL);
  free (request);

  assert (ai_configure ("http://gpustack.test:8090/v1/chat/completions",
                        "test-key") == TRUE);
  assert (ai_build_request ("He said \"hello\"\n", "glm-5.3",
                            1024, &request, &length) == TRUE);
  assert (strstr (request,
                  "POST /v1/chat/completions HTTP/1.0") != NULL);
  assert (strstr (request, "Host: gpustack.test:8090") != NULL);
  assert (strstr (request, "Authorization: Bearer test-key") != NULL);
  assert (strstr (request, "\"messages\":[{\"role\":\"system\"") != NULL);
  assert (strstr (request, "\"content\":\"He said \\\"hello\\\"\\n\"}")
          != NULL);
  assert (strstr (request, "\"role\":\"system\"") != NULL);
  assert (strstr (request, "\"role\":\"user\"") != NULL);
  assert (strstr (request,
                  "You are a helpful assistant inside MicroEMACS.") != NULL);
  assert (strstr (request, "\"max_tokens\":1024}") != NULL);
  free (request);

  assert (ai_set_reasoning_effort ("low") == TRUE);
  assert (ai_build_request ("He said \"hello\"\n", "glm-5.3",
                            32, &request, &length) == TRUE);
  assert (strstr (request, "\"reasoning_effort\":\"low\"") != NULL);
  free (request);
  assert (ai_set_reasoning_effort ("") == TRUE);
  assert (ai_build_request ("He said \"hello\"\n", "glm-5.3",
                            32, &request, &length) == TRUE);
  assert (strstr (request, "reasoning_effort") == NULL);
  free (request);
  assert (ai_set_reasoning_effort ("invalid") == FALSE);
  assert (strcmp (ai_last_error (), "Invalid AI reasoning effort") == 0);

  assert (ai_configure ("http://bad host/", NULL) == FALSE);
  assert (ai_configure ("https://example.test", NULL) == FALSE);
  assert (ai_configure ("http://example.test:70000", NULL) == FALSE);
  assert (ai_configure ("http://example.test/v1/chat/completions",
                        "bad\nkey") == FALSE);
  assert (ai_set_reasoning_effort ("low") == TRUE);
  assert (ai_configure ("http://127.0.0.1:11434/api/generate", NULL)
          == TRUE);
  assert (ai_reasoning_effort[0] == 0);

  memset (large_prompt, 'x', sizeof (large_prompt));
  large_prompt[sizeof (large_prompt) - 1] = 0;
  assert (ai_build_request (large_prompt, NULL, 0,
                            &request, &length) == TRUE);
  assert (length > sizeof (large_prompt));
  free (request);

  assert (ai_parse_response ("HTTP/1.0 200 OK\r\n\r\n"
                             "{\"model\":\"x\",\"response\":\"ok\\n\"}",
                             sizeof ("HTTP/1.0 200 OK\r\n\r\n"
                                     "{\"model\":\"x\",\"response\":\"ok\\n\"}")
                             - 1, output, sizeof (output)) == TRUE);
  assert (strcmp (output, "ok\n") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n\r\n"
                             "{\"meta\":{\"response\":\"nested\"},"
                             "\"response\":\"A\\u00e9\\n\\ud83d\\ude00\","
                             "\"done\":true}",
                             sizeof ("HTTP/1.1 200 OK\r\n\r\n"
                                     "{\"meta\":{\"response\":\"nested\"},"
                                     "\"response\":\"A\\u00e9\\n\\ud83d\\ude00\","
                                     "\"done\":true}") - 1,
                             output, sizeof (output)) == TRUE);
  assert (strcmp (output, "A\xc3\xa9\n\xf0\x9f\x98\x80") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n\r\n"
                             "{\"response\":\"\"}",
                             sizeof ("HTTP/1.1 200 OK\r\n\r\n"
                                     "{\"response\":\"\"}") - 1,
                             output, sizeof (output)) == FALSE);
  assert (strcmp (ai_last_error (), "AI response content is empty") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n\r\n"
                             "{\"response\":\" \\n\\t \"}",
                             sizeof ("HTTP/1.1 200 OK\r\n\r\n"
                                     "{\"response\":\" \\n\\t \"}") - 1,
                             output, sizeof (output)) == FALSE);
  assert (strcmp (ai_last_error (), "AI response content is empty") == 0);

  assert (ai_parse_response ("HTTP/1.0 500 Internal Server Error\r\n\r\n{}",
                             sizeof ("HTTP/1.0 500 Internal Server Error\r\n\r\n{}")
                             - 1, output, sizeof (output)) == FALSE);
  assert (strcmp (ai_last_error (), "AI HTTP status 500") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n"
                             "Transfer-Encoding: chunked\r\n\r\n"
                             "d\r\n{\"response\": \r\n"
                             "f\r\n\"chunked text\"}\r\n0\r\n\r\n",
                             sizeof ("HTTP/1.1 200 OK\r\n"
                                     "Transfer-Encoding: chunked\r\n\r\n"
                                     "d\r\n{\"response\": \r\n"
                                     "f\r\n\"chunked text\"}\r\n0\r\n\r\n")
                             - 1, output, sizeof (output)) == TRUE);
  assert (strcmp (output, "chunked text") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n\r\n"
                             "{\"choices\":[{\"index\":0,"
                             "\"message\":{\"role\":\"assistant\","
                             "\"content\":\"chat ok\"}}]}",
                             sizeof ("HTTP/1.1 200 OK\r\n\r\n"
                                     "{\"choices\":[{\"index\":0,"
                                     "\"message\":{\"role\":\"assistant\","
                                     "\"content\":\"chat ok\"}}]}")
                             - 1, output, sizeof (output)) == TRUE);
  assert (strcmp (output, "chat ok") == 0);

  assert (ai_parse_response ("HTTP/1.1 200 OK\r\n\r\n"
                             "{\"choices\":[{\"index\":0,"
                             "\"message\":{\"role\":\"assistant\","
                             "\"content\":\"\"}}]}",
                             sizeof ("HTTP/1.1 200 OK\r\n\r\n"
                                     "{\"choices\":[{\"index\":0,"
                                     "\"message\":{\"role\":\"assistant\","
                                     "\"content\":\"\"}}]}") - 1,
                             output, sizeof (output)) == FALSE);
  assert (strcmp (ai_last_error (), "AI response content is empty") == 0);

  {
    static const char prefix[] = "HTTP/1.0 200 OK\r\n\r\n{\"response\":\"";
    size_t prefix_length = sizeof (prefix) - 1;
    size_t response_length = sizeof (output) - 1;
    size_t http_length;
    char response[8192];
    char http_response[8192];

    memset (response, 'x', response_length);
    response[response_length] = 0;
    memcpy (http_response, prefix, prefix_length);
    memcpy (http_response + prefix_length, response, response_length);
    http_length = prefix_length + response_length;
    http_response[http_length] = '"';
    http_response[http_length + 1] = 0;
    assert (ai_parse_response (http_response, http_length + 2,
                               output, sizeof (output)) == TRUE);
    assert (strlen (output) == response_length);
  }

  printf ("ai-test passed\n");
  return 0;
}
