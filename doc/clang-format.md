clang-format
============

## Custom style example

```
## .clang-format file

---
BasedOnStyle: GNU
ColumnLimit: 120
IndentWidth: 2
TabWidth: 4
UseCRLF: false
UseTab: Never
BreakBeforeBraces: Stroustrup
AllowShortIfStatementsOnASingleLine: false
IndentCaseLabels: false
---
```

## BS_Custom example

```
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass:            true/false
  AfterControlStatement: true/false
  AfterEnum:             true/false
  AfterFunction:         true/false
  AfterNamespace:        true/false
  AfterObjCDeclaration:  true/false
  AfterStruct:           true/false
  AfterUnion:            true/false
  BeforeCatch:           true/false
  BeforeElse:            true/false
  IndentBraces:          true/false
```
