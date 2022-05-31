#include "dustsrc_term.hpp"

#include "../../time_step/time_step.hpp"
#include "../../util/util.hpp"
#include "../hll_dust.hpp"
#include "../../mesh/mesh.hpp"


#ifdef CARTESIAN_COORDINATE
void dust_terminalvelocityapprixmation_xyz(double &vg1, double &vg2, double &vg3,
                                           double &g1,  double &g2,  double &g3,
                                           double &rhod, double &ts,
                                           double &pd1, double &pd2, double &pd3){
    pd1 = rhod * vg1 + g1 * ts;
    pd2 = rhod * vg2 + g2 * ts;
    pd3 = rhod * vg3 + g3 * ts;
    }
#endif // COORDINATE

#ifdef SPHERICAL_POLAR_COORD
void dust_terminalvelocityapprixmation_rtp(double &vg1, double &vg2, double &vg3,
                                           double &g1,  double &g2,  double &g3,
                                           double &rhod, double &ts,  double &r, double &cottheta,
                                           double &pd1, double &pd2, double &pd3){
    pd1 = rhod * vg1 + (g1 + rhod * (vg2 * vg2 + vg3 * vg3) / r) * ts;
    pd2 = rhod * vg2 + (g2 - rhod * (vg1 * vg2 - vg3 * vg3 * cottheta) / r) * ts;
    pd3 = rhod * vg3 + (g3 - rhod * (vg1 * vg3 + vg2 * vg3 * cottheta) / r) * ts;
    }
#endif // COORDINATE


void apply_source_terms_dust(mesh &m, double &dt, BootesArray<double> &stoppingtimemesh){
    #if defined (CARTESIAN_COORDINATE)
        #pragma omp parallel for collapse (4)
        for (int specIND  = 0; specIND < m.NUMSPECIES; specIND++){
            for (int kk = m.x3s; kk < m.x3l; kk ++){
                for (int jj = m.x2s; jj < m.x2l; jj ++){
                    for (int ii = m.x1s; ii < m.x1l; ii ++){
                        // gravity
                        double rhogradphix1;
                        double rhogradphix2;
                        double rhogradphix3;
                        #ifdef ENABLE_GRAVITY
                        rhogradphix1 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x1surface(kk, jj, ii + 1) - m.grav->Phi_grav_x1surface(kk, jj, ii)) / m.dx1p(kk, jj, ii);
                        rhogradphix2 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x2surface(kk, jj + 1, ii) - m.grav->Phi_grav_x2surface(kk, jj, ii)) / m.dx2p(kk, jj, ii);
                        rhogradphix3 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x3surface(kk + 1, jj, ii) - m.grav->Phi_grav_x3surface(kk, jj, ii)) / m.dx3p(kk, jj, ii);
                        #else   // set gravity to zero
                        rhogradphix1 = 0;
                        rhogradphix2 = 0;
                        rhogradphix3 = 0;
                        #endif // ENABLE_GRAVITY

                        if (stoppingtimemesh(specIND, kk, jj, ii) < dt){
                            dust_terminalvelocityapprixmation_xyz(m.prim(IV1, kk, jj, ii), m.prim(IV2, kk, jj, ii), m.prim(IV3, kk, jj, ii),
                                                                  rhogradphix1, rhogradphix2, rhogradphix3,
                                                                  m.dprim(specIND, IDN, kk, jj, ii), stoppingtimemesh(specIND, kk, jj, ii),
                                                                  m.dcons(specIND, IM1, kk, jj, ii), m.dcons(specIND, IM2, kk, jj, ii), m.dcons(specIND, IM3, kk, jj, ii)
                                                                  );
                        }
                        else{
                            // apply gravity
                            m.dcons(specIND, IM1, kk, jj, ii) += rhogradphix1 * dt;
                            m.dcons(specIND, IM2, kk, jj, ii) += rhogradphix2 * dt;
                            m.dcons(specIND, IM3, kk, jj, ii) += rhogradphix3 * dt;
                            // gas drag
                            double vdust1 = m.dprim(specIND, IV1, kk, jj, ii);
                            double vgas1  = m.prim(IV1, kk, jj, ii);
                            double vdust2 = m.dprim(specIND, IV2, kk, jj, ii);
                            double vgas2  = m.prim(IV2, kk, jj, ii);
                            double vdust3 = m.dprim(specIND, IV3, kk, jj, ii);
                            double vgas3  = m.prim(IV3, kk, jj, ii);
                            double rhodt_stime = m.dprim(specIND, IDN, kk, jj, ii) * dt / stoppingtimemesh(specIND, kk, jj, ii);
                            double dragMOM1 = rhodt_stime * (vgas1 - vdust1);
                            double dragMOM2 = rhodt_stime * (vgas2 - vdust2);
                            double dragMOM3 = rhodt_stime * (vgas3 - vdust3);
                            m.dcons(specIND, IM1, kk, jj, ii) += dragMOM1;
                            m.dcons(specIND, IM2, kk, jj, ii) += dragMOM2;
                            m.dcons(specIND, IM3, kk, jj, ii) += dragMOM3;
                        }
                    }
                }
            }
        }
    #elif defined (SPHERICAL_POLAR_COORD)
        #pragma omp parallel for collapse (4)
        for (int specIND  = 0; specIND < m.NUMSPECIES; specIND++){
            for (int kk = m.x3s; kk < m.x3l; kk ++){
                for (int jj = m.x2s; jj < m.x2l; jj ++){
                    for (int ii = m.x1s; ii < m.x1l; ii ++){
                        double rhogradphix1;
                        double rhogradphix2;
                        double rhogradphix3;
                        #ifdef ENABLE_GRAVITY
                        rhogradphix1 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x1surface(kk, jj, ii + 1) - m.grav->Phi_grav_x1surface(kk, jj, ii)) / m.dx1p(kk, jj, ii);
                        rhogradphix2 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x2surface(kk, jj + 1, ii) - m.grav->Phi_grav_x2surface(kk, jj, ii)) / m.dx2p(kk, jj, ii);
                        rhogradphix3 = m.dprim(specIND, IDN, kk, jj, ii) * (m.grav->Phi_grav_x3surface(kk + 1, jj, ii) - m.grav->Phi_grav_x3surface(kk, jj, ii)) / m.dx3p(kk, jj, ii);
                        #else   // set gravity to zero
                        rhogradphix1 = 0;
                        rhogradphix2 = 0;
                        rhogradphix3 = 0;
                        #endif // ENABLE_GRAVITY

                        if (stoppingtimemesh(specIND, kk, jj, ii) < dt){
                            dust_terminalvelocityapprixmation_rtp(m.prim(IV1, kk, jj, ii), m.prim(IV2, kk, jj, ii), m.prim(IV3, kk, jj, ii),
                                                                  rhogradphix1, rhogradphix2, rhogradphix3,
                                                                  m.dprim(specIND, IDN, kk, jj, ii), stoppingtimemesh(specIND, kk, jj, ii), m.x1v(ii), m.geo_cot(jj),
                                                                  m.dcons(specIND, IM1, kk, jj, ii), m.dcons(specIND, IM2, kk, jj, ii), m.dcons(specIND, IM3, kk, jj, ii)
                                                                  );
                        }
                        else {
                            // apply gravity
                            m.dcons(specIND, IM1, kk, jj, ii) += rhogradphix1 * dt;
                            m.dcons(specIND, IM2, kk, jj, ii) += rhogradphix2 * dt;
                            m.dcons(specIND, IM3, kk, jj, ii) += rhogradphix3 * dt;
                            // gas drag
                            double vdust1 = m.dprim(specIND, IV1, kk, jj, ii);
                            double vgas1  = m.prim(IV1, kk, jj, ii);
                            double vdust2 = m.dprim(specIND, IV2, kk, jj, ii);
                            double vgas2  = m.prim(IV2, kk, jj, ii);
                            double vdust3 = m.dprim(specIND, IV3, kk, jj, ii);
                            double vgas3  = m.prim(IV3, kk, jj, ii);
                            double rhodt_stime = m.dprim(specIND, IDN, kk, jj, ii) * dt / stoppingtimemesh(specIND, kk, jj, ii);
                            double dragMOM1 = rhodt_stime * (vgas1 - vdust1);
                            double dragMOM2 = rhodt_stime * (vgas2 - vdust2);
                            double dragMOM3 = rhodt_stime * (vgas3 - vdust3);
                            m.dcons(specIND, IM1, kk, jj, ii) += dragMOM1;
                            m.dcons(specIND, IM2, kk, jj, ii) += dragMOM2;
                            m.dcons(specIND, IM3, kk, jj, ii) += dragMOM3;
                        }
                    }
                }
            }
        }
    #endif // defined (coordinate)
}





