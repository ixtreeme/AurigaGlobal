#pragma once

namespace AntiHook::AntiTamper
{
    // Inicializálás:
    //  - minden whitelistes modul .text szekcióját felderíti
    //  - baseline hash-t vesz róluk
    //  - PAGE_EXECUTE_READ-re teszi õket (ahol lehet)
    void Init();

    // Folyamatos ellenõrzés:
    //  - .text hash-ek összehasonlítása a baseline-nal
    //  - fõ exe IAT táblájának vizsgálata (import mutat-e az elvárt DLL-en kívülre)
    //  - gyanú esetén log + TerminateProcess
    void Scan(const char* logFile);
}
