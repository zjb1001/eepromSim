---
name: Safety Report
about: Report safety-related concerns or ISO 26262 compliance issues
title: '[SAFETY] '
labels: safety, critical
assignees: ''
---

## Safety Concern Description

*Describe the safety concern clearly and concisely:*

This template is for reporting safety-related issues that may affect ISO 26262 ASIL-B compliance.

## Severity Assessment

*How severe is this safety concern?*

* [ ] **Critical** - Could lead to system failure or safety violation
* [ ] **High** - Significant impact on safety mechanisms
* [ ] **Medium** - Moderate safety concern
* [ ] **Low** - Minor safety issue

## Affected Component

*Which part of the system is affected?*

* [ ] NvM core module
* [ ] EEPROM driver
* [ ] MemIf layer
* [ ] Block management (Native/Redundant/Dataset)
* [ ] Concurrent safety mechanisms (Seqlock/ABA prevention)
* [ ] Error handling
* [ ] Fault detection/recovery
* [ ] Other: [please specify]

## Safety Mechanism Impact

*Which safety mechanisms are affected?*

* [ ] CRC16 checksums
* [ ] Redundant storage
* [ ] Seqlock concurrency control
* [ ] ABA problem prevention
* [ ] Error detection and reporting
* [ ] Power loss recovery
* [ ] Wear leveling
* [ ] Other: [please specify]

## Evidence

*Please provide evidence of the safety concern:*

* **Test Case**: [Which test demonstrates the issue?]
* **Log Output**:
```
// Paste relevant logs
```
* **Code Location**: [File and line numbers]

## AUTOSAR Compliance

*Does this affect AUTOSAR compliance?*

* [ ] Yes, violates AUTOSAR specification
* [ ] No, but needs improvement
* [ ] Unclear

**Specification Section**: [if applicable]

## Proposed Solution

*How do you think this should be fixed?*

## Additional Context

*Any other safety-related information:*

* **ISO 26262 Part**: [e.g., Part 6 (Product development at software level)]
* **ASIL Impact**: [Does this affect ASIL-B compliance?]
* **Mitigation**: [Current mitigation if any]

---

**Note**: Safety-related issues are prioritized and will be addressed promptly.
