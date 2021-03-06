// Check the basic reporting/warning and the application of constraints.
// RUN: %clang_analyze_cc1 %s \
// RUN:   -analyzer-checker=core \
// RUN:   -analyzer-checker=apiModeling.StdCLibraryFunctions \
// RUN:   -analyzer-checker=apiModeling.StdCLibraryFunctionArgs \
// RUN:   -analyzer-checker=debug.StdCLibraryFunctionsTester \
// RUN:   -analyzer-checker=debug.ExprInspection \
// RUN:   -triple x86_64-unknown-linux-gnu \
// RUN:   -verify=report

// Check the bugpath related to the reports.
// RUN: %clang_analyze_cc1 %s \
// RUN:   -analyzer-checker=core \
// RUN:   -analyzer-checker=apiModeling.StdCLibraryFunctions \
// RUN:   -analyzer-checker=apiModeling.StdCLibraryFunctionArgs \
// RUN:   -analyzer-checker=debug.StdCLibraryFunctionsTester \
// RUN:   -analyzer-checker=debug.ExprInspection \
// RUN:   -triple x86_64-unknown-linux-gnu \
// RUN:   -analyzer-output=text \
// RUN:   -verify=bugpath

void clang_analyzer_eval(int);

int glob;

#define EOF -1

int isalnum(int);

void test_alnum_concrete(int v) {
  int ret = isalnum(256); // \
  // report-warning{{Function argument constraint is not satisfied}} \
  // bugpath-warning{{Function argument constraint is not satisfied}} \
  // bugpath-note{{Function argument constraint is not satisfied}}
  (void)ret;
}

void test_alnum_symbolic(int x) {
  int ret = isalnum(x);
  (void)ret;

  clang_analyzer_eval(EOF <= x && x <= 255); // \
  // report-warning{{TRUE}} \
  // bugpath-warning{{TRUE}} \
  // bugpath-note{{TRUE}} \
  // bugpath-note{{Left side of '&&' is true}} \
  // bugpath-note{{'x' is <= 255}}

}

void test_alnum_symbolic2(int x) {
  if (x > 255) { // \
    // bugpath-note{{Assuming 'x' is > 255}} \
    // bugpath-note{{Taking true branch}}

    int ret = isalnum(x); // \
    // report-warning{{Function argument constraint is not satisfied}} \
    // bugpath-warning{{Function argument constraint is not satisfied}} \
    // bugpath-note{{Function argument constraint is not satisfied}}

    (void)ret;
  }
}

typedef struct FILE FILE;
typedef typeof(sizeof(int)) size_t;
size_t fread(void *, size_t, size_t, FILE *);
void test_notnull_concrete(FILE *fp) {
  fread(0, sizeof(int), 10, fp); // \
  // report-warning{{Function argument constraint is not satisfied}} \
  // bugpath-warning{{Function argument constraint is not satisfied}} \
  // bugpath-note{{Function argument constraint is not satisfied}}
}
void test_notnull_symbolic(FILE *fp, int *buf) {
  fread(buf, sizeof(int), 10, fp);
  clang_analyzer_eval(buf != 0); // \
  // report-warning{{TRUE}} \
  // bugpath-warning{{TRUE}} \
  // bugpath-note{{TRUE}} \
  // bugpath-note{{'buf' is not equal to null}}
}
void test_notnull_symbolic2(FILE *fp, int *buf) {
  if (!buf) // bugpath-note{{Assuming 'buf' is null}} \
            // bugpath-note{{Taking true branch}}
    fread(buf, sizeof(int), 10, fp); // \
    // report-warning{{Function argument constraint is not satisfied}} \
    // bugpath-warning{{Function argument constraint is not satisfied}} \
    // bugpath-note{{Function argument constraint is not satisfied}}
}

int __two_constrained_args(int, int);
void test_constraints_on_multiple_args(int x, int y) {
  // State split should not happen here. I.e. x == 1 should not be evaluated
  // FALSE.
  __two_constrained_args(x, y);
  clang_analyzer_eval(x == 1); // \
  // report-warning{{TRUE}} \
  // bugpath-warning{{TRUE}} \
  // bugpath-note{{TRUE}}
  clang_analyzer_eval(y == 1); // \
  // report-warning{{TRUE}} \
  // bugpath-warning{{TRUE}} \
  // bugpath-note{{TRUE}}
}

int __arg_constrained_twice(int);
void test_multiple_constraints_on_same_arg(int x) {
  __arg_constrained_twice(x);
  // Check that both constraints are applied and only one branch is there.
  clang_analyzer_eval(x < 1 || x > 2); // \
  // report-warning{{TRUE}} \
  // bugpath-warning{{TRUE}} \
  // bugpath-note{{TRUE}} \
  // bugpath-note{{Assuming 'x' is < 1}} \
  // bugpath-note{{Left side of '||' is true}}
}
