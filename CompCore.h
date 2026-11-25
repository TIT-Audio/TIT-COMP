#pragma once
// Núcleo del compresor TIT COMP (sin dependencias de JUCE): curva estática, detector y balística por modo.
#include <algorithm>
#include <cmath>

namespace titc
{
enum Mode { Clean = 0, VCA, FET, FETB, OPTO, VARI, MU, NumModes };

struct Settings
{
    int mode = Clean;
    double thr = -18.0, ratio = 4.0, knee = 6.0;
    double attackMs = 10.0, releaseMs = 120.0, holdMs = 0.0;
    double range = 60.0;     // reducción máxima en dB
    double link = 1.0;       // 0..1
    bool   autoRel = false;  // release automático (Clean y VCA TIT)
    // mandos propios de cada compresor original
    double fin = 36.0, fout = 19.0;   // FET: Input y Output (0..48)
    double gain = 26.0;               // opto: Gain 0..100
    double pr = 40.0;                 // opto: Peak Reduction 0..100
    double vin = 0.0, vthr = 10.0;    // variable-mu: Input Gain (atenuación 0..20) y Threshold 0..10
    // TIT MU (variable-mu de dos canales medido): Input Trim dB, Threshold 0..10, Attack posicion 0..2 (3 posiciones), Recovery posicion 0..4 (5 posiciones), Make Up 0..19 dB, Filtro sidechain Hz (0 = off)
    double muTrim = 0.0, muThr = 10.0, muAtt = 1.0, muRec = 2.0, muMk = 0.0, muFilt = 0.0;
    double outParam = 0.0;            // mando Out en dB (make-up / Output / Gain según el modo)
    double makeupDb = 0.0;            // resultado: ganancia de salida que se aplica
};


// ---- Límites de cada modo: solo se pueden mover los mandos como en el compresor original ----
struct ModeLocks { bool attack, release, ratio, knee, hold, autoRel; };   // true = el mando está bloqueado (autoRel: true = disponible)
inline ModeLocks locksFor (int mode) noexcept
{
    switch (mode)
    {
        case VCA:  return { false, false, false, true,  true,  true  };   // VCA bus: ataque, release y ratio por pasos; rodilla fija; Auto
        case FET:
        case FETB: return { false, false, false, true,  true,  false };   // FET: ataque y release continuos dentro de su rango; ratio 4/8/12/20
        case OPTO: return { true,  true,  true,  true,  true,  false };   // opto: sin ataque ni release ni ratio seleccionables
        case VARI: return { true,  false, true,  true,  true,  false };   // variable-mu: ataque fijo; release = 6 posiciones de Time Constant
        case MU:   return { true,  true,  true,  true,  true,  false };   // TIT MU: mandos propios (Attack y Recovery de 0 a 10)
        default:   return { false, false, false, false, false, true  };   // Clean: libre, con Auto Release
    }
}
inline double snapLog (double x, const double* t, int n) noexcept
{
    double best = t[0];
    for (int i = 1; i < n; ++i)
        if (std::abs (std::log (std::max (1.0e-9, x) / t[i])) < std::abs (std::log (std::max (1.0e-9, x) / best))) best = t[i];
    return best;
}
// Ajusta los valores a lo que permite el original (el procesador lo aplica siempre; el editor refleja el resultado en los mandos)
inline void constrain (Settings& s) noexcept
{
    static const double vcaA[] = { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 }, vcaR[] = { 100.0, 300.0, 600.0, 1200.0 }, vcaRat[] = { 2.0, 4.0, 10.0 };
    static const double fetRat[] = { 4.0, 8.0, 12.0, 20.0 }, variTc[] = { 100.0, 200.0, 800.0, 1100.0, 1500.0, 2500.0 };
    s.autoRel = s.autoRel && (s.mode == VCA || s.mode == Clean);
    switch (s.mode)
    {
        case VCA:
            s.attackMs = snapLog (s.attackMs, vcaA, 6); s.releaseMs = snapLog (s.releaseMs, vcaR, 4);
            s.ratio = snapLog (s.ratio, vcaRat, 3); s.knee = 4.0; s.holdMs = 0.0; break;
        case FET:
        case FETB:
            s.attackMs = std::clamp (s.attackMs, 0.02, 0.8); s.releaseMs = std::clamp (s.releaseMs, 50.0, 1100.0);
            s.ratio = snapLog (s.ratio, fetRat, 4); s.knee = 3.0; s.holdMs = 0.0; break;
        case OPTO:
            s.attackMs = 10.0; s.releaseMs = 1500.0; s.ratio = 3.0; s.knee = 12.0; s.holdMs = 0.0; break;
        case MU:
            s.attackMs = 1.0; s.releaseMs = 500.0; s.ratio = 2.0; s.knee = 20.0; s.holdMs = 0.0;
            s.muTrim = std::clamp (s.muTrim, -24.0, 24.0); s.muThr = std::clamp (s.muThr, 0.0, 10.0); s.muAtt = std::clamp (std::round (s.muAtt), 0.0, 2.0);
            s.muRec = std::clamp (std::round (s.muRec), 0.0, 4.0); s.muMk = std::clamp (s.muMk, 0.0, 19.0);
            s.muFilt = s.muFilt < 25.0 ? 0.0 : std::clamp (std::round (s.muFilt / 50.0) * 50.0, 50.0, 250.0); break;
        case VARI:
            s.attackMs = 1.5; s.releaseMs = snapLog (s.releaseMs, variTc, 6); s.ratio = 6.5; s.knee = 9.0; s.holdMs = 0.0; break;
        default: break;
    }
}

// Traduce los mandos propios de cada original a umbral interno (dBFS) y ganancia de salida (medido en un DAW con los plugins originales)
inline void derive (Settings& s) noexcept
{
    s.makeupDb = s.outParam;
    switch (s.mode)
    {
        case VCA:   // VCA bus: Threshold libre (dBFS directos); Make-up -5..+15 como el original
            s.makeupDb = std::clamp (s.outParam, -5.0, 15.0); break;
        case FET:
        case FETB:
        {   // FET: Input 0..48 (atenuación; más bajo = más compresión) y Output 0..48 (más bajo = más fuerte, ~0.83 dB por unidad)
            static const double ix[] = { 0.0, 12.0, 28.0, 36.0, 48.0 }, iy[] = { -60.0, -42.0, -30.0, -14.2, -2.0 };
            const double x = 48.0 - std::clamp (s.fin, 0.0, 48.0); int i = 1; while (x > ix[i]) ++i;   // Input: derecha = mas entrada = mas compresion
            s.thr = iy[i - 1] + (x - ix[i - 1]) / (ix[i] - ix[i - 1]) * (iy[i] - iy[i - 1]);
            s.makeupDb = (s.fout - 29.0) * 0.834; break;   // Output: derecha = mas nivel
        }
        case OPTO:
        {   // opto: Peak Reduction (medido en 35/50/62; en 0 no comprime) y Gain independiente de la detección:
            // con Gain 0 no suena nada pero la reducción sigue; con Peak Reduction al mínimo no hay compresión. Gain medido: 40 -> +6.7 dB, 100 -> +34.7 dB
            static const double px[] = { 0.0, 5.0, 10.0, 35.0, 50.0, 62.0, 100.0 }, py[] = { 20.0, 3.0, -9.5, -21.5, -28.9, -34.4, -52.6 };
            const double x = std::clamp (s.pr, 0.0, 100.0); int i = 1; while (x > px[i]) ++i;
            s.thr = py[i - 1] + (x - px[i - 1]) / (px[i] - px[i - 1]) * (py[i] - py[i - 1]);
            const double g = std::clamp (s.gain, 0.0, 100.0);
            const double at10 = 0.467 * (10.0 - 25.6);
            s.makeupDb = g < 10.0 ? -120.0 + (g / 10.0) * (at10 + 120.0) : 0.467 * (g - 25.6);
            break;
        }
        case VARI:
        {   // compresor variable-mu: Input Gain = atenuación 1 dB por unidad (0..20); Threshold 0..10 con curva medida; +1.58 dB de ganancia fija
            static const double tx[] = { 0.0, 2.5, 5.0, 7.5, 10.0 }, ty[] = { 3.0, -13.8, -22.0, -24.1, -27.0 };
            const double att = 20.0 - std::clamp (s.vin, 0.0, 20.0);   // Input Gain: derecha = mas entrada
            const double x = std::clamp (s.vthr, 0.0, 10.0); int i = 1; while (x > tx[i]) ++i;
            s.thr = ty[i - 1] + (x - tx[i - 1]) / (tx[i] - tx[i - 1]) * (ty[i] - ty[i - 1]) + att;
            s.makeupDb = 1.58 + std::clamp (s.outParam, -20.0, 20.0) - att; break;
        }
        case MU:
        {   // Threshold 0..10: cada unidad por debajo del maximo sube el umbral ~4.5 dB (medido 8.5 dB cada 2 unidades); Input Trim entra antes de la curva
            const double shift = (10.0 - std::clamp (s.muThr, 0.0, 10.0)) * 4.5;
            s.thr = shift - s.muTrim;
            s.makeupDb = s.muMk + s.muTrim; break;
        }
        default: break;
    }
}

// Rodilla efectiva: los modos "suaves" no bajan de un mínimo
inline double effectiveKnee (const Settings& s) noexcept
{
    double w = std::max (0.0, s.knee);
    if (s.mode == OPTO) w = std::max (w, 10.0);
    if (s.mode == VARI) w = std::max (w, 9.0);   // variable-mu medido: umbral ~-27 dBFS, rodilla ~8-9 dB, ratio ~6.5
    return w;
}

// Interpolación logarítmica en una tabla (x creciente)
inline double logTable (double x, const double* xs, const double* ys, int n) noexcept
{
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    for (int i = 1; i < n; ++i)
        if (x <= xs[i])
        {
            const double t = std::log (x / xs[i - 1]) / std::log (xs[i] / xs[i - 1]);
            return ys[i - 1] + t * (ys[i] - ys[i - 1]);
        }
    return ys[n - 1];
}

// Curva estática: reducción de ganancia en dB (>= 0) para un nivel de entrada x en dB
inline double gainReductionDb (double x, const Settings& s) noexcept
{
    if (s.mode == MU)
    {   // curva medida (Threshold al maximo): reduccion absoluta (dB) segun el nivel de entrada; por encima de -2 dBFS sigue con pendiente 0.5 (2:1)
        static const double lx[] = { -52.0, -48.0, -44.0, -40.0, -36.0, -32.0, -28.0, -24.0, -20.0, -16.0, -12.0, -8.0, -4.0, -2.0 };
        static const double ly[] = {   0.0,   0.15,  0.5,   1.03,  1.73,  2.67,  4.29,  6.2,   8.4,   11.2,  13.2,  15.2, 17.2, 17.97 };
        const double v = x - s.thr;
        double gr;
        if (v <= lx[0]) gr = 0.0;
        else if (v >= lx[13]) gr = ly[13] + 0.5 * (v - lx[13]);
        else { int i = 1; while (v > lx[i]) ++i; gr = ly[i - 1] + (v - lx[i - 1]) / (lx[i] - lx[i - 1]) * (ly[i] - ly[i - 1]); }
        return std::clamp (gr, 0.0, std::max (0.0, s.range));
    }
    double R = std::max (1.0, s.ratio);
    if (s.mode == VCA) R = std::pow (R, 0.85);
    if (s.mode == FET) R = std::pow (R, 1.1);    // FET (LN) medido: la pendiente efectiva de los botones 4/8/12/20 sube a ~5/10/15/27:1   // medido en el VCA bus: ratio 2/4/10 se comporta como ~1.8/3.3/7
    double thr = s.thr;
    if (s.mode == FET || s.mode == FETB)
    {   // FET medido (LN y Rev A): al subir el boton de ratio (4/8/12/20) el umbral efectivo sube ~0/2.9/3.8/5.3 dB
        static const double rx[] = { 4.0, 8.0, 12.0, 20.0 }, ry[] = { 0.0, 2.9, 3.8, 5.3 };
        thr += logTable (std::max (1.0, s.ratio), rx, ry, 4);
    }
    const double W = effectiveKnee (s), over = x - thr;
    double out;
    if (W < 0.01)                         out = over <= 0.0 ? x : thr + over / R;
    else if (2.0 * over < -W)             out = x;
    else if (2.0 * std::abs (over) <= W)  out = x + (1.0 / R - 1.0) * (over + 0.5 * W) * (over + 0.5 * W) / (2.0 * W);
    else                                  out = thr + over / R;
    return std::clamp (x - out, 0.0, std::max (0.0, s.range));
}

// Detector de potencia: tiempos de subida y bajada (ms) y corrección para que un seno a fondo de escala lea 0 dB
struct DetectorTimes { double atkMs, relMs, offsetDb; };
inline DetectorTimes detectorFor (int mode) noexcept
{
    switch (mode)
    {
        case VCA:  return { 1.0, 1.0,  3.01 };
        case FET:  return { 0.02, 3.0,  0.0 };
        case FETB: return { 0.02, 3.0,  0.0 };
        case OPTO: return { 10.0, 10.0, 3.01 };
        case VARI: return { 1.0, 1.0,  3.01 };
        case MU:   return { 1.0, 1.0,  3.01 };
        default:   return { 0.05, 5.0,  0.0 };
    }
}

struct Ballistics
{
    double e = 0.0, ef = 0.0, es = 0.0, mem = 0.0;
    int hold = 0;
    void reset() noexcept { e = ef = es = mem = 0.0; hold = 0; }
};

struct Coefs
{
    bool dual = false, mu = false;
    double muRate = 7.0;   // TIT MU: velocidad de recuperacion en dB por segundo (rampa lineal)
    double aA = 0.0, aAs = 0.0, aR = 0.0;
    int holdSamples = 0;
    double aRf = 0.0, tauSlowBase = 1.0, k0 = 1.0, k1 = 0.0, wFast = 0.5, memCoef = 0.0, fs = 44100.0;
};

inline Coefs makeCoefs (const Settings& s, double fs) noexcept
{
    Coefs c;
    c.fs = fs;
    const bool fet = (s.mode == FET || s.mode == FETB);
    double atkMs = std::max (fet ? 0.02 : 0.01, s.attackMs);
    if (s.mode == VCA)
    {   // medido en el VCA bus (mando 0.1/0.3/1/3/10/30 ms): la reducción llega al 63 % en ~1/1/2/6/16/37 ms
        static const double kx[] = { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 }, ky[] = { 0.5, 0.8, 1.5, 4.5, 13.0, 34.0 };
        atkMs = logTable (atkMs, kx, ky, 6);
    }
    if (fet)
    {   // FET medido (Rev E): del mando mas rapido al mas lento la reduccion llega al 63 % en ~0.15 ms .. ~3 ms
        const double u = std::clamp ((atkMs - 0.02) / 0.78, 0.0, 1.0);
        const double extra = std::max (0.0, atkMs - 0.8);
        // FET TIT: FET (LN) (0.15 .. 3 ms); FET TIT B: Rev A, algo mas lento (0.4 .. 4.5 ms)
        atkMs = (s.mode == FETB ? 0.4 + 4.1 * u : 0.15 + 4.0 * u) + extra;   // por encima de 0.8 ms (fuera del rango del original) sigue sumando
    }
    if (s.mode == VARI) atkMs = 1.5;   // medido en un compresor variable-mu: ataque ~1.5 ms en todas las posiciones de Time Constant
    c.aA = std::exp (-1.0 / (atkMs * 0.001 * fs));
    c.aAs = c.aA;
    // medido en un compresor de bus tipo VCA bus: la constante de liberación es ~0.42 x el valor del mando (exponencial única)
    double relMs = std::max (1.0, s.releaseMs);
    if (s.mode == VCA)
    {   // mando 0.1/0.3/0.6/1.2 s -> constante 90/131/267/567 ms (exponencial única)
        static const double rx[] = { 100.0, 300.0, 600.0, 1200.0 }, ry[] = { 90.0, 131.0, 267.0, 567.0 };
        relMs = (relMs >= 1200.0) ? relMs * (567.0 / 1200.0) : logTable (relMs, rx, ry, 4);
    }
    c.aR = std::exp (-1.0 / (std::max (1.0, relMs) * 0.001 * fs));
    c.holdSamples = (int) std::lround (std::max (0.0, s.holdMs) * 0.001 * fs);
    c.memCoef = 1.0 / (8.0 * fs);

    if (s.autoRel && (s.mode == VCA || s.mode == Clean))
    {   // VCA bus en Auto (medido): arranque rápido pequeño (~11 % en 50 ms) y cola lenta de ~2 s
        c.dual = true; c.wFast = 0.1;
        c.aRf = std::exp (-1.0 / (0.05 * fs));
        c.tauSlowBase = 2.0; c.k0 = 1.0; c.k1 = 0.0;
    }
    else if (s.mode == OPTO)
    {   // célula óptica: la liberación es rápida al principio y luego lenta, y depende de cuánto tiempo lleva comprimiendo
        // medido en un opto (Peak Reduction ~50): ataque en dos etapas (~75 % en 5 ms, el resto tarda ~0.3 s),
        // liberación rápida ~80 ms + cola lenta ~1.3 s (con el mando en 1500 ms), memoria pequeña
        c.dual = true; c.wFast = 0.58;
        c.aAs = std::exp (-1.0 / (0.3 * fs));
        c.aRf = std::exp (-1.0 / (0.08 * fs));
        c.tauSlowBase = std::max (1.0, s.releaseMs) * 0.001 * (1.3 / 1.5); c.k0 = 0.9; c.k1 = 0.2;
    }
    else if (s.mode == FETB)
    {   // FET (Rev A) medido: el mando lento da una exponencial de ~1.05 s (meseta inicial ~50 ms), el medio ~0.9 s,
        // el rapido ~0.27 s y el mas rapido de dos etapas (~0.13 s y cola de ~0.3 s)
        static const double rx[] = { 50.0, 120.0, 800.0, 1100.0 }, ry[] = { 0.30, 0.27, 0.90, 1.05 };
        const double rel = std::clamp (s.releaseMs, 50.0, 1100.0);
        c.dual = true;
        c.wFast = rel < 120.0 ? 0.7 * (120.0 - rel) / 70.0 : 0.0;
        c.aRf = std::exp (-1.0 / (0.13 * fs));
        c.tauSlowBase = logTable (rel, rx, ry, 4); c.k0 = 1.0; c.k1 = 0.0;
    }
    else if (fet)
    {   // FET (LN) medido: liberacion con parte rapida (~0.1 s, hasta el 75 % con el mando rapido) y cola de ~0.9-1.25 s
        const double u = std::log (std::clamp (s.releaseMs, 50.0, 1100.0) / 50.0) / std::log (1100.0 / 50.0);   // 0 rapido, 1 lento
        c.dual = true; c.wFast = 0.75 * (1.0 - u);
        c.aRf = std::exp (-1.0 / (0.1 * fs));
        c.tauSlowBase = 0.9 + 0.35 * u; c.k0 = 1.0; c.k1 = 0.0;
    }
    else if (s.mode == VARI)
    {   // compresor variable-mu medido: posiciones 1/3/5 -> liberacion ~0.1/0.7/1.5 s (con una pequena meseta inicial);
        // la posicion 6 es de dos etapas: ~0.25 s y luego ~2.5 s
        const double rel = std::max (1.0, s.releaseMs);
        if (rel < 2500.0)
        {
            static const double rx[] = { 100.0, 200.0, 800.0, 1100.0, 1500.0, 2500.0 }, ry[] = { 100.0, 100.0, 700.0, 1300.0, 1500.0, 2000.0 };
            c.aR = std::exp (-1.0 / (logTable (rel, rx, ry, 6) * 0.001 * fs));
        }
        else
        {
            c.dual = true; c.wFast = 0.5;
            c.aRf = std::exp (-1.0 / (0.25 * fs));
            c.tauSlowBase = 2.5; c.k0 = 1.0; c.k1 = 0.0;
        }
    }
    if (s.mode == MU)
    {   // TIT MU medido (tono -6 dBFS tras -60 dBFS, sin contaminar):
        // Attack pos. 0: <1 ms; pos. 1: ~1 ms; pos. 2: 31 % inmediato + cola de ~36 ms (63 % a los ~24 ms)
        // Recovery: 5 posiciones, rampa lineal de 31 / 15.4 / 7.3 / 3.65 / 1.8 dB/s con una pequena espera inicial (30/45/15/100/400 ms)
        static const double wf[] = { 1.0, 1.0, 0.31 }, tauF[] = { 0.3, 0.65, 0.5 }, tauS[] = { 1.0, 1.0, 36.0 };
        static const double rate[] = { 31.0, 15.4, 7.3, 3.65, 1.8 }, holdMs[] = { 30.0, 45.0, 15.0, 100.0, 400.0 };
        const int a = (int) std::clamp (std::round (s.muAtt), 0.0, 2.0), r = (int) std::clamp (std::round (s.muRec), 0.0, 4.0);
        c.mu = true; c.wFast = wf[a];
        c.aA = std::exp (-1.0 / (tauF[a] * 0.001 * fs));
        c.aAs = std::exp (-1.0 / (tauS[a] * 0.001 * fs));
        c.muRate = rate[r];
        c.holdSamples = (int) std::lround (holdMs[r] * 0.001 * fs);
    }
    return c;
}

// Un paso de balística sobre la reducción objetivo g (dB). Devuelve la reducción suavizada (dB).
inline double step (Ballistics& b, double g, const Coefs& c) noexcept
{
    if (c.mu)
    {
        const double dn = c.muRate / c.fs;
        if (g >= b.e - 1.0e-6) b.hold = c.holdSamples;
        const bool waiting = (g < b.e - 1.0e-6) && b.hold > 0;
        if (waiting) --b.hold;
        if (g > b.ef) b.ef = c.aA * b.ef + (1.0 - c.aA) * g; else if (! waiting) b.ef = std::max (g, b.ef - dn);
        if (g > b.es) b.es = c.aAs * b.es + (1.0 - c.aAs) * g; else if (! waiting) b.es = std::max (g, b.es - dn);
        b.e = c.wFast * b.ef + (1.0 - c.wFast) * b.es;
        return b.e;
    }
    if (! c.dual)
    {
        if (g > b.e)            { b.e = c.aA * b.e + (1.0 - c.aA) * g; b.hold = c.holdSamples; }
        else if (b.hold > 0)    --b.hold;
        else                    b.e = c.aR * b.e + (1.0 - c.aR) * g;
        return b.e;
    }
    const double tauS = c.tauSlowBase * (c.k0 + c.k1 * b.mem);
    const double aRs = 1.0 - 1.0 / (std::max (1.0e-3, tauS) * c.fs);
    if (g > b.ef) b.ef = c.aA * b.ef + (1.0 - c.aA) * g; else b.ef = c.aRf * b.ef + (1.0 - c.aRf) * g;
    if (g > b.es) b.es = c.aAs * b.es + (1.0 - c.aAs) * g; else b.es = aRs   * b.es + (1.0 - aRs)   * g;
    b.e = c.wFast * b.ef + (1.0 - c.wFast) * b.es;
    b.mem += (std::clamp (b.e / 10.0, 0.0, 1.0) - b.mem) * c.memCoef;
    return b.e;
}
} // namespace titc
