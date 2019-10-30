#ifndef GAMMA_H_
#define GAMMA_H_

/* Gamma correction table */
extern uint8_t GAMMA_TABLE[];

void updateGammaTable(float gammaVal, float briteVal);

#endif /* GAMMA_H_ */
