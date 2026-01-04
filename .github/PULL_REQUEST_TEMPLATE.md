## Pull Request Checklist

Please ensure your PR meets the following requirements:

* [ ] **Code compiles without errors or warnings**
* [ ] **All existing tests pass**
* [ ] **New tests added for new functionality** (if applicable)
* [ ] **Documentation updated** (README, design docs, or code comments)
* [ ] **Commit messages follow our guidelines**
* [ ] **No merge conflicts** with the main branch
* [ ] **Commits are squashed** into logical units (one feature/fix per commit)

---

## Type of Change

* [ ] **Bug fix** - Non-breaking change that fixes an issue
* [ ] **New feature** - Non-breaking change that adds functionality
* [ ] **Breaking change** - Fix or feature that would cause existing functionality to not work as expected
* [ ] **Documentation update** - Documentation only changes
* [ ] **Refactoring** - Code change that neither fixes a bug nor adds a feature
* [ ] **Performance improvement** - Code change that improves performance
* [ ] **Test improvement** - Addition or improvement of tests

---

## Description

### Summary

*Briefly describe what this PR does and why it's needed.*

### Related Issue

*Fixes #<issue_number>* or *Related to #<issue_number>*

---

## Changes Made

### Code Changes

*List the main files modified:*
* `file_path_1` - Description of changes
* `file_path_2` - Description of changes

### Documentation Changes

*List documentation updates:*
* `docs/file_name.md` - What was updated

### Test Changes

*List test additions or modifications:*
* `tests/test_module.c` - New test cases added

---

## AUTOSAR Compliance

*If this changes affects AUTOSAR compliance:*

* [ ] **Changes follow AUTOSAR 4.3 specifications**
* [ ] **API changes are documented**
* [ ] **Specification reference provided** (if applicable)

**Specification Section**: `if applicable`

---

## Safety Impact Analysis

*Given this is an ISO 26262 ASIL-B compliant project:*

### Safety Mechanisms

*Does this change affect any safety mechanisms?*

* [ ] **No** - No impact on safety mechanisms
* [ ] **Yes** - Impacts:
  * [ ] CRC16 checksums
  * [ ] Redundant storage
  * [ ] Seqlock concurrency control
  * [ ] ABA problem prevention
  * [ ] Error detection and reporting
  * [ ] Power loss recovery
  * [ ] Other: [please specify]

### Testing

*What testing has been done to ensure safety is maintained?*

* [ ] **Unit tests** - All passing: `__ / __`
* [ ] **Integration tests** - All passing: `__ / __`
* [ ] **Fault injection tests** - All passing: `__ / __`
* [ ] **Manual testing** - Description of manual tests performed

### Verification

*How have you verified this change doesn't compromise safety?*

---

## Testing Instructions

### How to Test

*Provide step-by-step instructions on how to test this change:*

```bash
# Build instructions
cd build && make clean && make all

# Run specific tests
cd tests/unit && make test

# Run example program (if applicable)
./examples/basic/exXX_example_name.bin
```

### Expected Results

*Describe what should happen when the change is working correctly:*

### Test Environment

* **OS**: [e.g. Linux 5.15, Ubuntu 22.04]
* **Compiler**: [e.g. GCC 11.3.0]
* **Build Type**: [Debug/Release]

---

## Performance Impact

*Does this change affect performance?*

* [ ] **No performance impact**
* [ ] **Improved performance** - Describe improvement
* [ ] **Minor performance impact** - Acceptable trade-off for benefit
* [ ] **Performance concern** - Needs review

**Benchmarks**: `if applicable`

---

## Screenshots / Demo Output

*If this is a visual change or adds new functionality, add screenshots or demo output:*

```
// Paste terminal output or use screenshots for web-based tools
```

---

## Checklist for Specific Changes

### For Bug Fixes
* [ ] Bug is clearly described in linked issue
* [ ] Fix doesn't break existing functionality
* [ ] Regression tests added or updated

### For New Features
* [ ] Feature is documented in README or design docs
* [ ] Example program included (if applicable)
* [ ] Error handling implemented
* [ ] Edge cases considered

### For Documentation Changes
* [ ] Technical accuracy verified
* [ ] Grammar and spelling checked
* [ ] Code examples tested
* [ ] Links and references verified

### For Refactoring
* [ ] Behavior unchanged (verified by tests)
* [ ] Code is more readable/maintainable
* [ ] No unnecessary changes included

---

## Additional Notes

*Any additional information, context, or considerations:*

---

## Reviewer Notes

*Specific areas where you'd like reviewers to focus:*

* "Please pay special attention to..."
* "I'm not sure about..."
* "Alternative approaches considered:..."

---

**Thank you for your contribution! 🎉**

We appreciate your effort to improve eepromSim. Maintainers will review your PR as soon as possible.
