#ifndef EWTS_NGEN_BRIDGE_H
#define EWTS_NGEN_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

void ewts_ngen_log(const char* ewts_id, int level, const char* message);

void ewts_ngen_payload_status(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);

#ifdef __cplusplus
}
#endif

#endif /* EWTS_NGEN_BRIDGE_H */
