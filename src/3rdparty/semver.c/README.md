# semver.c [![Build Status](https://travis-ci.org/h2non/semver.c.png)](https://travis-ci.org/h2non/semver.c) [![GitHub release](https://img.shields.io/github/tag/h2non/semver.c.svg)](https://github.com/h2non/semver.c/releases)

[Semantic version](http://semver.org) v2.0 parser and render written in [ANSI C](https://en.wikipedia.org/wiki/ANSI_C) with zero dependencies.

## Features

- [x] Standard compliant (otherwise, open an issue)
- [x] Version metadata parsing
- [x] Version prerelease parsing
- [x] Version comparison helpers
- [x] Supports comparison operators
- [x] Version render
- [x] Version bump
- [x] Version sanitizer
- [x] 100% test coverage
- [x] No regexp (ANSI C doesn't support it)
- [x] Numeric conversion for sorting/filtering

## Versions

- [v0](https://github.com/h2non/semver.c/tree/89e66f36544e0250def32640b84b7e15c8585da4) - Legacy version. Beta. Not maintained anymore.
- [v1](https://github.com/h2non/semver.c) - Current stable version.

## Usage

Basic comparison:
```c
#include <stdio.h>
#include <semver.h>

char current[] = "1.5.10";
char compare[] = "2.3.0";

int
main(int argc, char *argv[]) {
    semver_t current_version = {};
    semver_t compare_version = {};

    if (semver_parse(current, &current_version)
      || semver_parse(compare, &compare_version)) {
      fprintf(stderr,"Invalid semver string\n");
      return -1;
    }

    int resolution = semver_compare(compare_version, current_version);

    if (resolution == 0) {
      printf("Versions %s is equal to: %s\n", compare, current);
    }
    else if (resolution == -1) {
      printf("Version %s is lower than: %s\n", compare, current);
    }
    else {
      printf("Version %s is higher than: %s\n", compare, current);
    }

    // Free allocated memory when we're done
    semver_free(&current_version);
    semver_free(&compare_version);
    return 0;
}
```

Satisfies version:

```c
#include <stdio.h>
#include <semver.h>

semver_t current = {};
semver_t compare = {};

int
main(int argc, char *argv[]) {
    semver_parse("1.3.10", &current);
    semver_parse("1.5.2", &compare);

    // Use caret operator for the comparison
    char operator[] = "^";

    if (semver_satisfies(current, compare, operator)) {
      printf("Version %s can be satisfied by %s", "1.3.10", "1.5.2");
    }

    // Free allocated memory when we're done
    semver_free(&current);
    semver_free(&compare);
    return 0;
}
```

## Installation

Clone this repository:

```bash
$ git clone https://github.com/h2non/semver.c
```

Or install with [clib](https://github.com/clibs/clib):

```bash
$ clib install h2non/semver.c
```

## API

#### struct semver_t { int major, int minor, int patch, char * prerelease, char * metadata }

semver base struct.

#### semver_parse(const char *str, semver_t *ver) => int

Parses a string as semver expression.

**Returns**:

- `-1` - In case of invalid semver or parsing error.
- `0` - All was fine!

#### semver_compare(semver_t a, semver_t b) => int

Compare versions `a` with `b`.

Returns:
- `-1` in case of lower version.
- `0` in case of equal versions.
- `1` in case of higher version.

#### semver_satisfies(semver_t a, semver_t b, char *operator) => int

Checks if both versions can be satisfied
based on the given comparison operator.

**Allowed operators**:

- `=`  - Equality
- `>=` - Higher or equal to
- `<=` - Lower or equal to
- `<`  - Lower than
- `>`  - Higher than
- `^`  - Caret operator comparison ([more info](https://docs.npmjs.com/misc/semver#caret-ranges-1-2-3-0-2-5-0-0-4))
- `~`  - Tilde operator comparison ([more info](https://docs.npmjs.com/misc/semver#tilde-ranges-1-2-3-1-2-1))

**Returns**:

- `1` - Can be satisfied
- `0` - Cannot be satisfied

#### semver_satisfies_caret(semver_t a, semver_t b) => int

Checks if version `x` can be satisfied by `y`
performing a comparison with caret operator.

See: https://docs.npmjs.com/misc/semver#caret-ranges-1-2-3-0-2-5-0-0-4

**Returns**:

- `1` - Can be satisfied
- `0` - Cannot be satisfied

#### semver_satisfies_patch(semver_t a, semver_t b) => int

Checks if version `x` can be satisfied by `y`
performing a comparison with tilde operator.

See: https://docs.npmjs.com/misc/semver#tilde-ranges-1-2-3-1-2-1

**Returns**:

- `1` - Can be satisfied
- `0` - Cannot be satisfied


#### semver_eq(semver_t a, semver_t b) => int

Equality comparison.

#### semver_ne(semver_t a, semver_t b) => int

Non equal comparison.

#### semver_gt(semver_t a, semver_t b) => int

Greater than comparison.

#### semver_lt(semver_t a, semver_t b) => int

Lower than comparison.

#### semver_gte(semver_t a, semver_t b) => int

Greater than or equal comparison.

#### semver_lte(semver_t a, semver_t b) => int

Lower than or equal comparison.

#### semver_render(semver_t *v, char *dest) => void

Render as string.

#### semver_numeric(semver_t *v) => int

Render as numeric value. Useful for ordering and filtering.

#### semver_bump(semver_t *a) => void

Bump major version.

#### semver_bump_minor(semver_t *a) => void

Bump minor version.

#### semver_bump_patch(semver_t *a) => void

Bump patch version.

#### semver_free(semver_t *a) => void

Helper to free allocated memory from heap.

#### semver_is_valid(char *str) => int

Checks if the given string is a valid semver expression.

#### semver_clean(char *str) => int

Removes invalid semver characters in a given string.

## License

MIT - Tomas Aparicio
