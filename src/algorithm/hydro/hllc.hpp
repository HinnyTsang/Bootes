#ifndef HLLC_HPP_
#define HLLC_HPP_

#include "../eos/eos.hpp"
#include <cmath>

void hllc(float rhoL, float pL, float uL,
          float rhoR, float pR, float uR,
          BootesArray<float> &valsL,
          BootesArray<float> &valsR,
          BootesArray<float> &fluxs,
          int INDRHO, int INDV1, int INDV2, int INDV3, int INDE,
          float gamma){
    /**
        rhoL, rhoR: density
        pL, pR:     pressure
        uL, uR:     speed
        valsL:      array of float storing the values on left. Quantities are <rho, rho*v1, rho*v2, rho*v3, E> (may not in this order)
        gamma:      hydro gamma
        INDRHO:     the ordering of variables in valsL and valsR (density)
        INDV1:      the ordering of variables in valsL and valsR (v1)
        INDV2:      the ordering of variables in valsL and valsR (v2)
        INDV3:      the ordering of variables in valsL and valsR (v3)
        INDE :      the ordering of variables in valsL and valsR (E)
    **/
    cout << pL << '\t' << pR << endl << flush;
    // step 1: pressure estimate
    float aL = soundspeed(rhoL, pL, gamma);
    float aR = soundspeed(rhoR, pR, gamma);
    float p_pvrs = 0.5 * (pL + pR) - 0.5 * (uR - uL) * 0.5 * (rhoL + rhoR) * 0.5 * (aL + aR);
    float pstar = max((float)0.0, p_pvrs);
    // step 2: wave speed estimates
    float qL, qR;
    if (pstar <= pL){
        qL = 1;
    }
    else{
        qL = sqrt(1 + (gamma + 1) / (2 * gamma) * (pstar / pL - 1));
    }
    if (pstar <= pR){
        qR = 1;
    }
    else{
        qR = sqrt(1 + (gamma + 1) / (2 * gamma) * (pstar / pR - 1));
    }
    float sL = uL - aL * qL;
    float sR = uR + aR * qR;

    float sstar = (pR - pL + rhoL * uL * (sL - uL) - rhoR * uR * (sR - uR)) / (rhoL * (sL - uL) - rhoR * (sR - uR));     // 10.70
    // step 3: HLLC flux
    float ustarL_secondhalf[5] = {
        1,
        sstar,
        valsL(INDV2),
        valsL(INDV3),
        valsL(INDE) / valsL(INDRHO) + (sstar - uL) * (sstar + pL / (pL * (sL - uL)))
    }; // 10.73
    float ustarR_secondhalf[5] = {
        1,
        sstar,
        valsR(INDV2),
        valsR(INDV3),
        valsR(INDE) / valsR(INDRHO) + (sstar - uR) * (sstar + pR / (pR * (sR - uR)))
    }; // 10.73
    for (int val_ind = 0; val_ind < 5; val_ind ++){
        float FL = valsL(val_ind) * uL;
        float FR = valsR(val_ind) * uR;
        float UstarL = valsL(val_ind) * (sL - uL) / (sL - sstar) * ustarL_secondhalf[val_ind];
        float UstarR = valsR(val_ind) * (sR - uR) / (sR - sstar) * ustarR_secondhalf[val_ind];
        if (0 <= sL){
            fluxs(val_ind) = FL;
        }
        else if (sL <= 0 <= sstar){
            fluxs(val_ind) = FL + sL * (UstarL - valsL(val_ind));
        }
        else if (sstar <= 0 <= sR){
            fluxs(val_ind) = FR + sR * (UstarR - valsR(val_ind));
        }
        else {
            fluxs(val_ind) = FR;
        }
    }
}

#endif // HLLC_HPP_
