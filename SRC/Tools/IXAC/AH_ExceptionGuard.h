#pragma once

namespace AntiHook::ExceptionGuard
{
    // Vectored Exception Handler telepítése.
    // Ezt hívd meg egyszer a kliens indulásakor (pl. AntiHook::Start() elején).
    void Init();

    // Handler eltávolítása – nem kötelezõ, de szép lezárás (pl. kliens zárásakor).
    void Shutdown();
}
