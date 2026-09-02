# Glossary

| Term | Definition |
| --- | --- |
| Component | A software package that supports the preparation, execution, or post-processing of ngen simulations. |
| Integrations | Layer containing specific integrations of the EWTS. Initial initigration is intended for ngen. |
| EWTS | Error and Warning Trapping System. |
| EWTS ID | Stable identifier for the module or component writing a message. |
| GFM | GitHub-flavored Markdown. |
| Mermaid | Text-based diagram syntax supported by MkDocs plugins or GitHub rendering contexts. |
| Module | A software component that implements a hydrologic or hydraulic process for use by ngen. |
| ngen | Next Generation Water Resources Modeling Framework runtime. |
| ngen bridge | Native integration layer that forwards EWTS messages into the ngen logging path. |
| Payload logging | Structured logging convention used to communicate progress and execution state in machine consumable form. |
| Payload message | A structured message wrapped in `<MSG_DATA>` `</MSG_DATA>` and containing JSON payload fields. |
| Standard logging | Structured logging convention used to communicate progress messages in a human readable form. |
| Standalone mode | Execution outside ngen bridge mode. |
| `STATUS` | EWTS log level used for payload messages. These are always logged regarless of the EWTS minimum log level. |
| RTE | Run Time Environment. The workflow management system used to execute and monitor ngen simulations across supported deployment environments, including developer workspaces, the WCOSS Test Cluster, and ngenCERF. |
| Runtime | Language-specific EWTS implementation used by modules and components. |
