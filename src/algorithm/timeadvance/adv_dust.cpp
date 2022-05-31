#include "adv_dust.hpp"

#include "../reconstruct/minmod_dust.hpp"
#include "../time_step/time_step.hpp"
#include "../util/util.hpp"
#include "../dust/hll_dust.hpp"
#include "../boundary_condition/apply_bc.hpp"
#include "../mesh/mesh.hpp"

void calc_flux_dust(mesh &m, double &dt, int &NUMSPECIES, BootesArray<double> &fdcons, BootesArray<double> &valsL, BootesArray<double> &valsR){
    // store the redconstructed value
        // index: (specIadvecting direction, quantity, kk, jj, ii)
    for (int axis = 0; axis < m.dim; axis ++){
        // step 1.1: redconstruct left/right values
        int x1excess, x2excess, x3excess;
        int IMP;        // Index of the velocity used in this axis
        if      (axis == 0){ x1excess = 1; x2excess = 0; x3excess = 0; IMP = IM1;}
        else if (axis == 1){ x1excess = 0; x2excess = 1; x3excess = 0; IMP = IM2;}
        else if (axis == 2){ x1excess = 0; x2excess = 0; x3excess = 1; IMP = IM3;}
        else { cout << "axis > 3!!!" << endl << flush; throw 1; }

        reconstruct_dust(m, valsL, valsR, x1excess, x2excess, x3excess, axis, IMP, dt);
        // step 1.2: solve the Riemann problem. Use HLL for now, update dconservative vars
        #pragma omp parallel for collapse (3) schedule (static)
        for (int specIND = 0; specIND < NUMSPECIES; specIND++){
            for (int kk = 0; kk < m.nx3 + x3excess; kk ++){
                for (int jj = 0; jj < m.nx2 + x2excess; jj ++){
                    for (int ii = 0; ii < m.nx1 + x1excess; ii ++){
                        double valL[4];
                        double valR[4];
                        double fxs[4];
                        valL[IDN] = valsL(specIND, axis, IDN, kk, jj, ii); valR[IDN] = valsR(specIND, axis, IDN, kk, jj, ii);
                        valL[IM1] = valsL(specIND, axis, IM1, kk, jj, ii); valR[IM1] = valsR(specIND, axis, IM1, kk, jj, ii);
                        valL[IM2] = valsL(specIND, axis, IM2, kk, jj, ii); valR[IM2] = valsR(specIND, axis, IM2, kk, jj, ii);
                        valL[IM3] = valsL(specIND, axis, IM3, kk, jj, ii); valR[IM3] = valsR(specIND, axis, IM3, kk, jj, ii);
                        hll_dust( valL, valR, fxs,
                             IMP,                   // the momentum term to add pressure; shift by one index (since first index is density)
                             m.hydro_gamma
                             );
                        fdcons(specIND, IDN, axis, kk, jj, ii) = fxs[IDN];
                        fdcons(specIND, IM1, axis, kk, jj, ii) = fxs[IM1];
                        fdcons(specIND, IM2, axis, kk, jj, ii) = fxs[IM2];
                        fdcons(specIND, IM3, axis, kk, jj, ii) = fxs[IM3];
                    }
                }
            }
        }
    }
    // Need to set unused values in the fdcons to zeros.
    // To do so the axis goes from "number of active axis" to 3
    #pragma omp parallel for collapse (5) schedule (static)
    for (int axis = m.dim; axis < 3; axis ++){
        for (int specIND = 0; specIND < NUMSPECIES; specIND++){
            for (int kk = 0; kk < fdcons.shape()[3]; kk ++){
                for (int jj = 0; jj < fdcons.shape()[4]; jj ++){
                    for (int ii = 0; ii < fdcons.shape()[5]; ii ++){
                        fdcons(specIND, IDN, axis, kk, jj, ii) = 0.0;
                        fdcons(specIND, IM1, axis, kk, jj, ii) = 0.0;
                        fdcons(specIND, IM2, axis, kk, jj, ii) = 0.0;
                        fdcons(specIND, IM3, axis, kk, jj, ii) = 0.0;
                    }
                }
            }
        }
    }
}


void advect_cons_dust(mesh &m, double &dt, int &NUMSPECIES, BootesArray<double> &fdcons, BootesArray<double> &valsL, BootesArray<double> &valsR){
    #if defined(CARTESIAN_COORD)
        #pragma omp parallel for collapse (4) schedule (static)
        for (int specIND  = 0; specIND < m.NUMSPECIES; specIND++){
            for (int kk = m.x3s; kk < m.x3l; kk ++){
                for (int jj = m.x2s; jj < m.x2l; jj ++){
                    for (int ii = m.x1s; ii < m.x1l; ii ++){
                        int kkf = kk - m.x3s;
                        int jjf = jj - m.x2s;
                        int iif = ii - m.x1s;
                        for (int dconsIND = 0; dconsIND < NUMCONS - 1; dconsIND++){
                            m.dcons(specIND, dconsIND, kk, jj, ii)
                                        -= (dt / m.dx1(ii) * (fdcons(specIND, dconsIND, 0, kkf, jjf, iif + 1) - fdcons(specIND, dconsIND, 0, kkf, jjf, iif))
                                          + dt / m.dx2(jj) * (fdcons(specIND, dconsIND, 1, kkf, jjf + 1, iif) - fdcons(specIND, dconsIND, 1, kkf, jjf, iif))
                                          + dt / m.dx3(kk) * (fdcons(specIND, dconsIND, 2, kkf + 1, jjf, iif) - fdcons(specIND, dconsIND, 2, kkf, jjf, iif)));
                        }
                    }
                }
            }
        }
    #elif defined(SPHERICAL_POLAR_COORD)
        #pragma omp parallel for collapse (3) schedule (static)
        for (int specIND  = 0; specIND < m.NUMSPECIES; specIND++){
            for (int kk = m.x3s; kk < m.x3l; kk ++){
                for (int jj = m.x2s; jj < m.x2l; jj ++){
                    for (int ii = m.x1s; ii < m.x1l; ii ++){
                        int kkf = kk - m.x3s;
                        int jjf = jj - m.x2s;
                        int iif = ii - m.x1s;
                        // first take care of the divergent terms
                        for (int dconsIND = 0; dconsIND < NUMCONS - 1; dconsIND++){
                            m.dcons(specIND, dconsIND, kk, jj, ii) -= (dt / m.vol(kk, jj, ii) * (fdcons(specIND, dconsIND, 0, kkf, jjf, iif + 1) * m.f1a(kk, jj, ii + 1) - fdcons(specIND, dconsIND, 0, kkf, jjf, iif) * m.f1a(kk, jj, ii))
                                                                     + dt / m.vol(kk, jj, ii) * (fdcons(specIND, dconsIND, 1, kkf, jjf + 1, iif) * m.f2a(kk, jj + 1, ii) - fdcons(specIND, dconsIND, 1, kkf, jjf, iif) * m.f2a(kk, jj, ii))
                                                                     + dt / m.vol(kk, jj, ii) * (fdcons(specIND, dconsIND, 2, kkf + 1, jjf, iif) * m.f3a(kk + 1, jj, ii) - fdcons(specIND, dconsIND, 2, kkf, jjf, iif) * m.f3a(kk, jj, ii)));
                        }

                        // geometry term
                        m.dcons(specIND, IM1, kk, jj, ii) += dt * m.one_orgeo(ii) * (m.dprim(IDN, kk, jj, ii) * (pow(m.dprim(IV2, kk, jj, ii), 2) + pow(m.dprim(IV3, kk, jj, ii), 2)));

                        m.dcons(specIND, IM2, kk, jj, ii) -= dt * m.dx1(ii) / m.rV(ii) * (m.rsq(ii) * valsL(specIND, 0, IM2, kkf, jjf, iif) + m.rsq(ii + 1) * valsR(0, IM2, kkf, jjf, iif + 1));
                        m.dcons(specIND, IM2, kk, jj, ii) += dt * m.geo_cot(jj) * m.one_orgeo(ii) * (m.dprim(IDN, kk, jj, ii) * pow(m.dprim(IV3, kk, jj, ii), 2));

                        m.dcons(specIND, IM3, kk, jj, ii) -= dt * m.dx1(ii) / m.rV(ii) * (m.rsq(ii) * valsL(specIND, 0, IM3, kkf, jjf, iif) + m.rsq(ii + 1) * valsR(0, IM3, kkf, jjf, iif + 1));
                        m.dcons(specIND, IM3, kk, jj, ii) -= dt * m.one_orgeo(ii) * m.geo_cot(jj) / (m.geo_sm(jj) + m.geo_sp(jj)) *
                                                                (m.geo_sm(jj) * valsL(specIND, 1, IM2, kkf, jjf, iif) * valsL(specIND, 1, IM3, kkf, jjf, iif)
                                                                + m.geo_sp(jj) * valsR(specIND, 1, IM2, kkf, jjf + 1, iif) * valsR(specIND, 1, IM3, kkf, jjf + 1, iif));



                    }
                }
            }
        }
    #else
        # error need coordinate defined
    #endif
}


#ifdef DUST_PROTECTION
void protection_dust(mesh &m){
    #pragma omp parallel for collapse (4)
    for (int specIND = 0; specIND < m.NUMSPECIES; specIND++){
        for (int kk = m.x3s; kk < m.x3l; kk ++){
            for (int jj = m.x2s; jj < m.x2l; jj ++){
                for (int ii = m.x1s; ii < m.x1l; ii ++){
                    if (isnan(m.dcons(specIND, IDN, kk, jj, ii)) || m.dcons(specIND, IDN, kk, jj, ii) < 1e-16){
                        m.dcons(specIND, IDN, kk, jj, ii) = 1e-16;
                    }
                    if (isnan(m.dcons(specIND, IM1, kk, jj, ii))){
                        m.dcons(specIND, IM1, kk, jj, ii) = 0;
                    }
                    if (isnan(m.dcons(specIND, IM2, kk, jj, ii))){
                        m.dcons(specIND, IM2, kk, jj, ii) = 0;
                    }
                    if (isnan(m.dcons(specIND, IM3, kk, jj, ii))){
                        m.dcons(specIND, IM3, kk, jj, ii) = 0;
                    }
                }
            }
        }
    }
}
#endif // DUST_PROTECTION



