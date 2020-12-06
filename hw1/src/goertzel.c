#include <stdint.h>
#include <math.h>

#include "debug.h"
#include "goertzel.h"
//make clean debug

void goertzel_init(GOERTZEL_STATE *gp, uint32_t N, double k) {
    // TO BE IMPLEMENTED
    // debug("Inside the goertzel_init() function!");
    //these calculations need to be confirmed
    //if k is supposed to be calculated, why is the value then passed in?
    // double calcK = k;
    // calcK = N * k / 8000;
    // if(calcK)
    // 	debug("k after calculations: %f", calcK);

    double A = 2*M_PI*(k/N);
    double B = 2*cos(A);

	GOERTZEL_STATE state = (GOERTZEL_STATE){.N = N, .k = k,
		.A = A, .B = B, .s0 = 0.0, .s1 = 0.0, .s2 = 0.0};
	*gp = state;
}

void goertzel_step(GOERTZEL_STATE *gp, double x) {
    // TO BE IMPLEMENTED
    // debug("Inside goertzel_step() function");
    GOERTZEL_STATE state = *gp;
    // x = x/INT16_MAX;
    // debug("x: %f", x);
    double s0 = x + (state.B * state.s1) - state.s2;
    double s2 = state.s1;
    double s1 = s0;
    // debug("s0 after calc: %f", s0);
    // debug("s1 after calc: %f", s1);
    // debug("s2 after calc: %f", s2);
    //update the typestruct with updated vals
    gp->s0 = s0;
    gp->s2 = s2;
    gp->s1 = s1;
}

double goertzel_strength(GOERTZEL_STATE *gp, double x) {
    // TO BE IMPLEMENTED
    // debug("Inside goertzel_strength()");
    // debug("%f\n", (*gp).s0);
    GOERTZEL_STATE state = *gp;
    // x = x/INT16_MAX;
    // debug("x: %f", x);
    double s0 = x + (state.B * state.s1) - state.s2;
    // debug("s0: %f", s0);
    //C has a real and imaginary part with cosA - jsinA
	double realC = cos(state.A);
	// debug("realC: %f", realC);
	double imaginaryC = -1*sin(state.A);
	// debug("imaginaryC: %f", imaginaryC);
	//divide y to real and imaginary parts
    double realY = s0 - (state.s1*realC); //a
    // debug("realY: %f", realY);
    double imaginaryY = fabs(state.s1*imaginaryC); //b
    // debug("imaginaryY: %f", imaginaryY);
    //now multiply by real and imaginary parts of D
    double realD = cos(((2*M_PI*state.k)/state.N)*(state.N-1)); //c
    double imaginaryD = -1*sin(((2*M_PI*state.k)/state.N)*(state.N-1)); //d
    // debug("realD: %f", realD);
    // debug("imaginaryD: %f", imaginaryD);
    //y = y*D
    double therealY = (realY*realD) - (imaginaryY*imaginaryD); //ac - bd
    double theimaginaryY = (realY*imaginaryD) + (imaginaryY*realD); //ad + bc
    // debug("realY after y = y * D: %f", realY);
    // debug("imaginaryY after y = y * D: %f", imaginaryY);
    // realY = fabs(realY * realD);
    // imaginaryY = fabs(imaginaryY * imaginaryD);

    //doing realY^2 + imaginaryY^2 is the squared magnitude
    //and we return 2 abs(y)^2
    double ans = 2*fabs((therealY*therealY)+(theimaginaryY*theimaginaryY))/(state.N*state.N);
    // debug("Answer: %f\n\n", ans);
    return ans;
}

