// Pruebas automaticas del nucleo del compresor (sin JUCE).
//  1) Curva estatica de cada modo: no debe cambiar por accidente (valores de referencia).
//  2) Ataque del TIT MU: coincide con las 5 posiciones medidas en un DAW, a 44.1, 48, 96 y 192 kHz.
#include "../CompCore.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check (bool ok, const char* what, double a, double b)
{
    if (! ok) { ++failures; std::printf ("FALLO: %s (%.3f vs %.3f)\n", what, a, b); }
}

int main()
{
    // ---- 1) curva estatica, entradas -60..0 dBFS de 6 en 6, ajustes por defecto de cada modo ----
    static const double golden[7][11] = {
        { 0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.5625,4.5000,9.0000,13.5000 },
        { 0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.3775,4.0263,8.0526,12.0789 },
        { 0.0000,0.0000,0.0000,0.2934,4.6942,9.3883,14.0825,18.7767,23.4709,28.1650,32.8592 },
        { 0.0000,0.0000,0.0000,0.2812,4.5000,9.0000,13.5000,18.0000,22.5000,27.0000,31.5000 },
        { 0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.9889,3.9778,7.9778,11.9778,15.9778 },
        { 0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,1.4220,5.9231 },
        { 0.0000,0.0000,0.1500,0.7650,1.7300,3.4800,6.2000,9.8000,13.2000,16.2000,18.9700 } };
    for (int m = 0; m < titc::NumModes; ++m)
    {
        titc::Settings s; s.mode = m; titc::constrain (s); titc::derive (s);
        for (int i = 0; i < 11; ++i)
            check (std::abs (titc::gainReductionDb (-60.0 + 6.0 * i, s) - golden[m][i]) < 0.01, "curva estatica", titc::gainReductionDb (-60.0 + 6.0 * i, s), golden[m][i]);
    }

    // ---- 2) ataque del TIT MU: reduccion (dB) a 2, 5, 10, 20, 50, 100, 200, 500 y 1000 ms de un escalon de 19.29 dB ----
    static const double tMs[9] = { 2, 5, 10, 20, 50, 100, 200, 500, 1000 };
    static const double meas[5][9] = {
        { 19.28, 19.28, 19.28, 19.28, 19.28, 19.28, 19.28, 19.28, 19.28 },
        { 18.21, 19.16, 19.28, 19.28, 19.28, 19.28, 19.28, 19.28, 19.28 },
        {  6.00,  7.79,  9.23, 10.70, 15.70, 17.42, 18.52, 19.23, 19.30 },
        {  6.88,  8.67, 10.10, 11.56, 15.93, 17.59, 18.64, 19.30, 19.30 },
        {  6.72,  8.50,  9.94, 11.41, 15.82, 17.88, 18.93, 19.30, 19.30 } };
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int a = 0; a < 5; ++a)
        {
            titc::Settings s; s.mode = titc::MU; s.muAtt = a; s.muRec = 2;
            titc::Coefs c = titc::makeCoefs (s, fs);
            titc::Ballistics b;
            int k = 0;
            const int n = (int) (1.0 * fs);
            for (int i = 1; i <= n && k < 9; ++i)
            {
                const double e = titc::step (b, 19.29, c);
                if (i == (int) std::lround (tMs[k] * 0.001 * fs))
                {
                    check (std::abs (e - meas[a][k]) < 1.0, "ataque MU", e, meas[a][k]);
                    ++k;
                }
            }
        }

    if (failures == 0) std::printf ("Todas las pruebas del nucleo han pasado.\n");
    return failures == 0 ? 0 : 1;
}
