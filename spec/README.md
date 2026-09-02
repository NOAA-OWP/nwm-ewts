# 📄 EWTS Specification Files

This directory contains the specification files used to generate language-specific constants and utilities for the EWTS logging system.

- `module_registry.yaml` — defines modules, their keys, and EWTS IDs  
- `log_levels.json` — defines log levels and their numeric values  

These files are consumed by the code generator:

```
tools/generate_language_constants.py
```

---

# 📦 `module_registry.yaml`

## Structure

The registry must be a YAML mapping with a top-level `modules` list:

```yaml
modules:
  - key: <string>
    ewts_id: <string>
    language: <string>
    description: <string>   # optional
```

---

## 🔑 `key` Rules

| Rule | Description |
|------|-------------|
| Required | Must be present and non-empty |
| Unique | Must be unique across all modules |
| Format | Flexible (normalized internally) |

### Normalization Behavior

- Non-alphanumeric characters → `_`  
- Multiple `_` collapsed  
- Leading/trailing `_` removed  
- If starting with a digit → prefixed with `M_`  

#### Examples

| Input Key | Generated Identifier |
|----------|---------------------|
| `t-route` | `T_ROUTE` |
| `forcing` | `FORCING` |
| `123abc` | `M_123ABC` |
| `t route` | `T_ROUTE` |

---

## 🆔 `ewts_id` Rules

| Rule | Description |
|------|-------------|
| Required | Must be present and non-empty |
| Uppercase | Must be entirely uppercase |
| Max Length | Maximum of **8 characters** |
| Duplicates Allowed | Multiple keys may share the same `ewts_id` |

#### Examples

| ewts_id | Valid |
|--------|------|
| `TROUTE` | ✅ |
| `FORCING` | ✅ |
| `troute` | ❌ (not uppercase) |
| `LONGMODULEID` | ❌ (> 8 chars) |

---

## 🔁 Key ↔ EWTS ID Relationships

- **One key → one EWTS ID**
- **One EWTS ID → many keys (allowed)**

---

## 🌐 `language` Rules

| Allowed Values |
|---------------|
| `c` |
| `cpp` |
| `fortran` |
| `python` |

---

## 📝 `description` (Optional)

- Free-form string
- Used in generated lookup tables and helper functions

---

# 📊 `log_levels.json`

## Structure

```json
{
  "version": <int>,
  "levels": {
    "<NAME>": <int>
  },
  "canonical_names": {
    "<int>": "<NAME>"
  }
}
```

---

## 📏 `levels` Rules

| Rule | Description |
|------|-------------|
| Required | Must exist and be non-empty |
| Keys | Must be non-empty strings |
| Values | Must be integers |

---

## 🎯 Default Log Level

The default level is determined as follows:

1. If `"INFO"` exists → used as default  
2. Else → fallback value if present  
3. Else → lowest numeric value  

---

## 🔍 Lookup Behavior

Generated helpers support:

- int → name  
- name → int  
- Case-insensitive parsing for string inputs  

---

## ⚠️ Aliases

- No alias support in parsing
- Only exact names in `levels` are recognized

---

## 🧾 `canonical_names`

- Optional mapping of numeric → canonical name
- Used for documentation or consistency

---

# ⚙️ Validation Summary

### `module_registry.yaml`

- Must contain `modules` list
- Each entry must include:
  - `key` (unique)
  - `ewts_id` (uppercase, ≤ 8 chars)
  - `language` (valid value)

---

### `log_levels.json`

- Must contain:
  - `levels` object (non-empty)
- Each level:
  - Name → string
  - Value → integer

---

# 🚀 Usage

Run the generator:

```bash
python tools/generate_language_constants.py
```
