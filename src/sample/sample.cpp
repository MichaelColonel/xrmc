/*
Copyright (C) 2017 Bruno Golosio

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
///////////////////////////////////
//     sample.cpp                //
//        01/12/2017             //
//   author : Bruno Golosio      //
///////////////////////////////////
// Methods of the classes sample and path
//

#include <iostream>
#include <cmath>
#include <cstring>
#include "xrmc_sample.h"
#include "xrmc_composition.h"
#include "xrmc_algo.h"
#include "xrmc_photon.h"
#include "xrmc_math.h"
#include "xrmc_exception.h"
#include <algorithm>
#include <xraylib.h>

#ifdef TIME_PERF
#include <ctime>
extern double outph_time0, edep_time, soutph_time, mu_time0, scatter_time,
				       phist_time, mu_time1, mux1_time,
				       psw_time, soutphx1_time, phist_time0;
#endif

using namespace std;
using namespace xrmc_algo;

// destructor
sample::~sample() { // destructor
  if (PhotonNum!=NULL) {
    delete[] PhotonNum;
  }
  if (Path!=NULL) delete Path;
  if (Geom3D!=NULL) delete Geom3D;
  if (Source!=NULL) delete Source;
}

void sample::DopplerInit() {
  //Doppler calculation

  doppler_pz = new double*[100];
  for (unsigned int i = 0 ; i < compZ.size() ; i++)
  	doppler_pz[compZ[i]] = new double[NINTERVALS_R];

  rs = new double[NINTERVALS_R];
  for (int i = 0 ; i < NINTERVALS_R ; i++)
  	rs[i] = i/(NINTERVALS_R-1.0);

  const int maxpz = 100;
  const int nintervals_pz = 1000000;
  double *pzs = new double[nintervals_pz];
  for (int i = 0 ; i < nintervals_pz ; i++) {
  	pzs[i]= maxpz * (i)/(nintervals_pz-1.0);
  }
  double *trapez2 = new double[nintervals_pz-1];
  double trapez2_sum, temp_sum;
  for (unsigned int i = 0 ; i < compZ.size() ; i++) {
  	cout << "Compton profile icdf for element: " << compZ[i] << endl;
  	trapez2_sum = 0.0;
	for (int j = 0 ; j < nintervals_pz-1 ; j++) {
		trapez2[j] = (ComptonProfile(compZ[i], pzs[j])+ComptonProfile(compZ[i],pzs[j+1]))*(pzs[1]-pzs[0])/2.0/compZ[i];
		trapez2_sum += trapez2[j];
	}

	for (int j = 0 ; j < nintervals_pz-1 ; j++) {
		trapez2[j] /= trapez2_sum;
	}
	
  	temp_sum = 0.0;
  	int l=0;
	int m=0;
	while (l != nintervals_pz-1) {
		temp_sum += trapez2[l];
		if (temp_sum >= rs[m]) {
			doppler_pz[compZ[i]][m] = pzs[l];
			if (m == NINTERVALS_R-1)
				break;
			m++;
		}
		if (l == nintervals_pz-2)
			break;
		l++;
	}
	doppler_pz[compZ[i]][NINTERVALS_R-1] = maxpz;
	doppler_pz[compZ[i]][0] = 0.0;
  }
  delete [] pzs;
  delete [] trapez2;

}

void sample::DopplerClear() {
  if (doppler_pz!=NULL) {
    for (unsigned int i = 0 ; i < compZ.size() ; i++) {
	delete [] doppler_pz[compZ[i]];
    }
    delete [] doppler_pz;
    doppler_pz = NULL;
  }
  if (rs != NULL) {
    delete [] rs;
    rs = NULL;	
  }
}

// constructor
sample::sample(string dev_name) {
  Runnable = false;
  NInputDevices = 3;
  InputDeviceCommand.push_back("SourceName");
  InputDeviceDescription.push_back("Source input device name");
  InputDeviceCommand.push_back("Geom3DName");
  InputDeviceDescription.push_back("Geom3D input device name");
  InputDeviceCommand.push_back("CompName");
  InputDeviceDescription.push_back("Composition input device name");

  PhotonNum = NULL;
  Path = NULL;
  Rng = NULL;
  doppler_pz = NULL;
  rs = NULL;
  SetDevice(dev_name, "sample");
}

//////////////////////////////////////////////////////////////////////
// method for casting input devices
// to basesource, geom3d and composition types
/////////////////////////////////////////////////////////////////////
int sample::CastInputDevices()
{
  // cast InputDevice[0] to type basesource*
  Source = dynamic_cast<basesource*>(InputDevice[0]);
  if (Source==0)
    throw xrmc_exception(string("Device ") + InputDeviceName[0] +
			 " cannot be casted to type basesource\n");

  // cast InputDevice[1] to type geom3d*
  Geom3D = dynamic_cast<geom3d*>(InputDevice[1]);
  if (Geom3D==0)
    throw xrmc_exception(string("Device ") + InputDeviceName[1] +
              " cannot be casted to type geom3d\n");

  // cast InputDevice[2] to type composition*
  Comp = dynamic_cast<composition*>(InputDevice[2]);
  if (Comp==0)
    throw xrmc_exception(string("Device ") + InputDeviceName[2] +
              " cannot be casted to type composition\n");

  return 0;
}

//////////////////////////////////////////////////////////////////////
// method for linking an input device
/////////////////////////////////////////////////////////////////////
/*
int sample::LinkInputDevice(string command, xrmc_device *dev_pt)
{
  if (command=="Geom3DName") {
    // cast it to type geom3d*
    Geom3D = dynamic_cast<geom3d*>(dev_pt);
    if (Geom3D==0)
      throw xrmc_exception(string("Device cannot be casted to type "
				  "geom3d\n"));
  }
  else if (command=="SourceName") {
    // cast it to type basesource*
    Source = dynamic_cast<basesource*>(dev_pt);
    if (Source==0)
      throw xrmc_exception(string("Device cannot be casted to type "
				  "basesource\n"));
  }
  else if (command=="CompName") {
    // cast it to type composition*
    Comp = dynamic_cast<composition*>(dev_pt);
    if (Comp==0)
      throw xrmc_exception(string("Device cannot be casted to type "
				  "composition\n"));
  }
  else
    throw xrmc_exception(string("Unrecognized command: ") + command + "\n");

  return 0;
}
*/

//////////////////////////////////////////////////////////////////////
// sample run initialization method
//////////////////////////////////////////////////////////////////////
int sample::RunInit()
{
  int mns;

  PhCFlag = false; 

  if (Path!=NULL) delete Path;
  Path = new path; // allocate the path member variable
  mns = 2*(Geom3D->QArr->NQuadr+4)+2; // maximum number of intersections
  Path->MaxNSteps = mns;            // (steplengths)
  Path->t = new double[mns]; // intersections (distances from starting point)
  Path->Step = new double[mns]; // steplengths between adjacent intersections
  Path->iPh0 = new int[mns]; // index of phase before intersection
  Path->iPh1 = new int[mns]; // index of phase after intersection
  Path->Mu = new double[mns]; // absorption coeff. in each step
  Path->Delta = new double[mns]; // delta coeff. in each step
  Path->SumMuS = new double[mns]; // cumulative sum of Mu * steplength
  Path->SumS = new double[mns]; // cumulative sum of steplengths

  int i, j;
  
  //get list of all used elements -> will be used for the Doppler broadening profiles
  for (i = 0 ; i < Geom3D->NQVol ; i++) {
  	//iterate over all qvolumes
	phase myPh = Geom3D->Comp->Ph[Geom3D->QVol[i].iPhaseIn];
	for (j = 0 ; j < myPh.NElem ; j++) {
		compZ.push_back(myPh.Z[j]);	
	}
	myPh = Geom3D->Comp->Ph[Geom3D->QVol[i].iPhaseOut];
	for (j = 0 ; j < myPh.NElem ; j++) {
		compZ.push_back(myPh.Z[j]);	
	}
  }
  sort(compZ.begin(), compZ.end());
  compZ.erase(unique(compZ.begin(), compZ.end()), compZ.end());
  return 0;
}

// initialize loop on events
int sample::Begin()
{
  Source->Begin();// initialize source loop on events
  ScattOrderIdx = PhotonIdx = 0; // set scatt. order index and event index to 0

  return 0;
}

// next step of the event loop
int sample::Next()
{
  if (PhCFlag) return Source->Next();
  // check if the loop is finished
  if (ScattOrderIdx >= ScattOrderNum) return 1;

  Source->Next();// next step of the source event loop
  if (Source->End()) { // check if the loop on source events is finished
    Source->Begin(); // if yes, reinitialize source loop on events, ...
    PhotonIdx++;     // increase the event index, ...
    // check if the event index reached the event multiplicity
    if (PhotonIdx >= PhotonNum[ScattOrderIdx]) {
      PhotonIdx = 0;   // if yes, reset the event index
      ScattOrderIdx++; // and start with the next scattering order
    }
  }
   
  return 0;
}

// check if the end of the loop is reached
bool sample::End()
{
  if (PhCFlag) return Source->End();

  if (ScattOrderIdx >= ScattOrderNum) return true;
  else return false;
}

// event multiplicity
long long sample::EventMulti()
{
  if (PhCFlag) return Source->EventMulti();
  int em = 0;
  for (int is=0; is<ScattOrderNum; is++) {
    em += PhotonNum[is];
  }

  return em*Source->EventMulti();
}

// number of scattering orders
int sample::ModeNum()
{
  return ScattOrderNum;
}

//////////////////////////////////////////////////////////////////////
// generate an event with a photon forced to end on the point x1
// and using ModeIdx orders of scattering
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon_x1(photon *Photon, vect3 x1, int *ModeIdx,
			  vect3 *prev_x)
{
  *ModeIdx = ScattOrderIdx; // set the scattering order
  return Out_Photon_x1(Photon, x1, prev_x); // generate the event
}


//////////////////////////////////////////////////////////////////////
// same as previous function but forces the photon to deposit energy
// in the end point
// and using ModeIdx orders of scattering
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon_x1(photon *Photon, vect3 x1, int *ModeIdx,
			  double *mu_x1, double *Edep, vect3 *prev_x)
{
  *ModeIdx = ScattOrderIdx; // set the scattering order
  return Out_Photon_x1(Photon, x1, mu_x1, Edep, prev_x); // generate the event
}

//////////////////////////////////////////////////////////////////////
// generate an event up to the last interaction position
// and using ModeIdx orders of scattering
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon(photon *Photon, int *ModeIdx)
{
  *ModeIdx = ScattOrderIdx; // set the scattering order
  return Out_Photon(Photon); // generate the event
}

//////////////////////////////////////////////////////////////////////
// same as previous function but forces the photon to deposit energy
// in the end point
// and using ModeIdx orders of scattering
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon(photon *Photon, int *ModeIdx, double *Edep)
{
  *ModeIdx = ScattOrderIdx; // set the scattering order
  return Out_Photon(Photon, Edep); // generate the event
}

//////////////////////////////////////////////////////////////////////
// method for evaluating the intersections of a straight trajectory
//  x0 + u*t with the sample
//////////////////////////////////////////////////////////////////////
int sample::Intersect(vect3 x0, vect3 u)
{
  int i;

  // find the intersections of the trajectory with the 3d objects
  Geom3D->Intersect(x0, u, Path->t, Path->iPh0, Path->iPh1, 
		    &Path->NSteps);
  for (i=0; i<Path->NSteps; i++) { // loop on the intersections
    // evaluate the steplengths
    Path->Step[i] = (i!=0) ? (Path->t[i] - Path->t[i-1]) : Path->t[0];
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////
// method for evaluating the absorption coefficient in each step
// of the intersections of a straight trajectory
//  x0 + u*t with the sample
//////////////////////////////////////////////////////////////////////
double sample::LinearAbsorption(vect3 x0, vect3 u)
{
  // find the intersections of the trajectory with the 3d objects
  Geom3D->Intersect(x0, u, Path->t, Path->iPh0, Path->iPh1, 
		    &Path->NSteps);
  Path->StepMu(Comp); // evaluate the absorption coefficient in each step

  return Path->MuL; // return the cumulative sum of Mu * steplength
}

//////////////////////////////////////////////////////////////////////
// Analogous to the previous function for t<tmax; tmax must be >= 0
//////////////////////////////////////////////////////////////////////
double sample::LinearAbsorption(vect3 x0, vect3 u, double tmax)
{
  int imax;

  // find the intersections of the trajectory with the 3d objects
  Geom3D->Intersect(x0, u, Path->t, Path->iPh0, Path->iPh1, 
		    &Path->NSteps);

  if (Path->NSteps>0 && tmax>=0) {
    if (tmax<Path->t[0]) {
      Path->NSteps = 1;
      Path->t[0] = tmax;
      Path->iPh1[0] = Path->iPh0[0];
    }
    else {
      Locate(tmax, Path->t, Path->NSteps, &imax); // locate tmax in the inters.
      if (imax < Path->NSteps-1) { // and eventually reduce the number of
	Path->NSteps = imax + 2;   // intersections and limit them to tmax
	Path->t[imax+1] = tmax;
	Path->iPh1[imax+1] = Path->iPh0[imax+1];
      }
      //else {
      //cout << "check that this is vacuum: "
      //     << Path->iPh1[Path->NSteps-1] << endl; // end point is in vacuum
      //Path->iPh1[Path->NSteps-1] = 1; // remove
      //}
    }
  }

  Path->StepMu(Comp); // evaluate the absorption coefficient in each step

  return Path->MuL; // return the cumulative sum of Mu * steplength
}


int sample::LinearMuDelta(vect3 x0, vect3 u)
{
  // find the intersections of the trajectory with the 3d objects
  Geom3D->Intersect(x0, u, Path->t, Path->iPh0, Path->iPh1, 
		    &Path->NSteps);
  Path->StepMuDelta(Comp); // evaluate the delta coefficient in each step

  return 0;
}

// generate a random position in the sample region
vect3 sample::RandomPoint()
{
  vect3 v = Geom3D->X; // center of the sample region
  /*Delete
  if (Rng == NULL) {
  	v.Elem[0] += (-1. + 2.*Rnd())*Geom3D->HW[0]; // uniform probability
  	v.Elem[1] += (-1. + 2.*Rnd())*Geom3D->HW[1]; // distribution
  	v.Elem[2] += (-1. + 2.*Rnd())*Geom3D->HW[2]; // in a parallelepiped
	} */
  //else {
  	v.Elem[0] += (-1. + 2.*Rnd_r(Rng))*Geom3D->HW[0]; // uniform probability
  	v.Elem[1] += (-1. + 2.*Rnd_r(Rng))*Geom3D->HW[1]; // distribution
  	v.Elem[2] += (-1. + 2.*Rnd_r(Rng))*Geom3D->HW[2]; // in a parallelepiped
  //}

  return 0;
}

//////////////////////////////////////////////////////////////////////
// evaluate the absorption coefficient at each step of the intersection
//////////////////////////////////////////////////////////////////////
int path::StepMu(composition *comp)
{
  int i;

  MuL = 0;

  for (i=0; i<NSteps; i++) { // loop on intersections
    Step[i] = (i!=0) ? (t[i] - t[i-1]) : t[0]; // steplength of intersection
    Mu[i] = comp->Ph[iPh0[i]].LastMu; // absorption coefficient of the phase
    MuL += Mu[i]*Step[i]; // cumulative sum of Mu * steplength
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////
// evaluate absorption and delta coefficients at each step of the intersection
//////////////////////////////////////////////////////////////////////
int path::StepMuDelta(composition *comp)
{
  int i;

  MuL = 0;
  DeltaL = 0;

  for (i=0; i<NSteps; i++) { // loop on intersections
    Step[i] = (i!=0) ? (t[i] - t[i-1]) : t[0]; // steplength of intersection
    Mu[i] = comp->Ph[iPh0[i]].LastMu; // absorption coefficient of the phase
    Delta[i] = comp->Ph[iPh0[i]].LastDelta; // delta coefficient of the phase
    MuL += Mu[i]*Step[i]; // cumulative sum of Mu * steplength
    DeltaL += Delta[i]*Step[i]; // cumulative sum of Delta * steplength
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////
// Extract the next interaction position using the standard MC approach
//////////////////////////////////////////////////////////////////////
double path::StepLength(int *step_idx, double *p_abs)
{
  double logargmin = 1e-10;
  double sum_mu_s, sum_s, step_length, R, mu_s_length, log1parg;
  int i, m;

  SumMuS[0] = SumS[0] = sum_mu_s = sum_s = 0;
  for (i=0; i<NSteps; i++) { // loop on intersections
    sum_mu_s += Mu[i]*Step[i];
    sum_s += Step[i];
    SumMuS[i+1] = sum_mu_s; // cumulative sum of mu * steplength
    SumS[i+1] = sum_s; // cumulative sum of steplengths
  }
  *p_abs = -expm1(-sum_mu_s); // total absorption probability
  
  do {
    // DELETE if (Rng == NULL)
    //R = Rnd(); // random number 0-1
    //else
    R = Rnd_r(Rng);
    log1parg = R*(*p_abs); // use the inverse cumulative distribution
  } while ((1.-log1parg)<logargmin); // check for logarith underflow
  
  mu_s_length = -log1p(-log1parg); // sum of mu*L up to the interaction position
  Locate(mu_s_length, SumMuS, NSteps, &m); // locate the step index
  *step_idx = m;
  double num = mu_s_length - SumMuS[m];
  double denom = Mu[m];
  if (num+denom==num) step_length=SumS[m+1]; // check for division overflow
  else {
    double s=num/denom; // path length to the interaction point in the last step
    step_length = SumS[m] + s; // total path length to the interaction point
  }
  
  return step_length; // distance from starting point to interaction point
}

///////////////////////////////////////////////////////////////////////////////
// Extract the next interaction position using the weighted steplength approach
///////////////////////////////////////////////////////////////////////////////
double path::WeightedStepLength(int *step_idx, double *weight)
{
  double sum, step_length, l, MuS, w;
  int i, m;

  m = (int)floor(Rnd_r(Rng)*NSteps);  // choose a step at random
  if (m >= NSteps) m = NSteps - 1;

  sum = 0;
  step_length = 0;
  for (i=0; i<m; i++) { // loop on all steps before the one extracted
    step_length += Step[i]; // sum of steplengths
    sum += Mu[i]*Step[i]; // sum of mu *steplength
  }
  MuS = Mu[m]*Step[m]; // mu * steplength on the step extracted
  w = exp(-sum)*NSteps;
  l = Rnd_r(Rng)*Step[m]; // random position on last step
  step_length += l; // total steplength up to the interaction position
  *weight = w*exp(-Mu[m]*l) * MuS; // weight of the event
  *step_idx = m;

  return step_length;
}


//////////////////////////////////////////////////////////////////////
// simulates the photon history up to the last interaction point
//////////////////////////////////////////////////////////////////////
int sample::PhotonHistory(photon *Photon, int &Z, int &interaction_type,
			  int scatt_order)
{
#ifdef TIME_PERF
  clock_t cpu_time = clock();
#endif
  Source->Out_Photon(Photon); // asks source to generate a photon
#ifdef TIME_PERF
  soutph_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
  cpu_time = clock();
#endif
  Comp->Mu(Photon->E); // absorption coefficients at photon energy
#ifdef TIME_PERF
  mu_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
  // loop on scattering interactions up to the scattering order
  cpu_time = clock();
#endif
  for (int is=1; is<=scatt_order; is++) {
    // Evaluates the photon next interaction type and position
    Photon->MonteCarloStep(this, &Z, &interaction_type);
    if (Photon->w == 0) break;
    if (is<scatt_order) { // check that it is not the last interaction
      // update the photon direction, polarization and energy
      Photon->Scatter(Z, interaction_type);
      // update absorption coefficients using the new energy value
      if (interaction_type != COHERENT) Comp->Mu(Photon->E);
    }   
  }
#ifdef TIME_PERF
  phist_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  
  return 0;
}

//////////////////////////////////////////////////////////////////////
// generate an event with a photon forced to end on the point x1
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon_x1(photon *Photon, vect3 x1, vect3 *prev_x)
{
  Out_Photon_x1(Photon, x1, ScattOrderIdx, prev_x);
  // divide the event weight by the multiplicity
  Photon->w /= PhotonNum[ScattOrderIdx];

  return 0;
}

//////////////////////////////////////////////////////////////////////
// generate an event with a photon forced to end on the point x1
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon_x1(photon *Photon, vect3 x1, int scatt_order,
			  vect3 *prev_x)
{
  int Z, interaction_type;
  vect3 vr;
  double tmax=0;

  if (PhotonNum[scatt_order]==0) {
    Photon->w = 0;
    return 0;
  }
  if (scatt_order == 0) { // transmission
    // ask the source to generate photon directed torward the point x1
    vect3 xs;
#ifdef TIME_PERF
    clock_t cpu_time = clock();
#endif
    Source->Out_Photon_x1(Photon, x1, &xs);
#ifdef TIME_PERF
    soutphx1_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
    Photon->x = xs; // send the photon back for now
                    // for proper calculation of survival probability
                    // done at the end of this method
    vr = x1 - xs; // end position relative to photon starting position
    tmax = vr.Mod(); // maximum intersection distance
    vr.Normalize(); // normalized direction
#ifdef TIME_PERF
    cpu_time = clock();
#endif
    Comp->Mu(Photon->E); // absorption coefficients at photon energy
#ifdef TIME_PERF
    mu_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  }
  else {
    double rwf;
    if (scatt_order==ScattOrderIdx) rwf = RWFractDet;
    else rwf = RWFract; 
    double rnd1 = Rnd_r(Rng);
    if (rnd1>rwf) {
#ifdef TIME_PERF
      clock_t cpu_time = clock();
#endif
      PhotonHistory(Photon, Z, interaction_type, scatt_order);
#ifdef TIME_PERF
      phist_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
      if (Photon->w == 0) return 0;
      vr = x1 - Photon->x; //end position relative to last interaction pos.
      tmax = vr.Mod(); // maximum intersection distance
      if (tmax<Rlim) {
	Photon->w = 0;
	return 0;
      }
      Photon->w *= 1.0/((1.0-rwf)*tmax*tmax);
    }
    else {
      double r;
      do {
	vr.Set(-1.0 + 2.0*Rnd_r(Rng),
	       -1.0 + 2.0*Rnd_r(Rng),
	       -1.0 + 2.0*Rnd_r(Rng));
	r = vr.Mod();
      } while (r>1.0);
      tmax = Rnd_r(Rng)*Rlim;
      if (tmax+r==tmax) {
	Photon->w = 0;
	return 0;
      }
      double fact = tmax/r;
      vect3 dx = vr*fact;
      vect3 x0 = x1 - dx;
      vect3 dummy_prev_x;
      
      Out_Photon_x1(Photon, x0, scatt_order-1, &dummy_prev_x);

      double mu_x1;
#ifdef TIME_PERF
      clock_t cpu_time = clock();
#endif
      Photon->Mu_x1(this, &Z, &interaction_type, &mu_x1);
#ifdef TIME_PERF
      mux1_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
      if (Photon->w == 0) return 0;

      // Photon->w *= Volume*mu_x1;
      Photon->w *= 4.0*PI*Rlim*mu_x1/rwf;
    }
    vr.Normalize(); // normalized direction
#ifdef TIME_PERF
    clock_t cpu_time = clock();
#endif
    // update the photon direction, polarization and energy
    // the photon is forced to go in the direction v_r
    Photon->Scatter(Z, &interaction_type, vr);
#ifdef TIME_PERF
    scatter_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
    // update absorption coefficients using the new energy value
    if (interaction_type != COHERENT) {
#ifdef TIME_PERF
      cpu_time = clock();
#endif
      Comp->Mu(Photon->E);
#ifdef TIME_PERF
      mu_time1 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
    }

  }
  Photon->uk = vr; // update the photon direction
#ifdef TIME_PERF
  clock_t cpu_time = clock();
#endif
  // weight the event with the survival probability
  PhotonSurvivalWeight(Photon, tmax);
#ifdef TIME_PERF
  psw_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  *prev_x = Photon->x;
  Photon->x = x1; // update the photon position

  return 0;
}

//////////////////////////////////////////////////////////////////////
// generate an event  up to the last interaction position
// and evaluates the energy deposition
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon(photon *Photon, double *Edep)
{
  int Z, interaction_type;

#ifdef TIME_PERF
  clock_t cpu_time = clock();
#endif
  Out_Photon(Photon);
  if (Photon->w == 0) {
    *Edep = 0;
    return 0;
  }
  //double E0 = Photon->E;
  if (ScattOrderIdx<ScattOrderNum-1) {
    // Evaluates the photon next interaction type and position
    Photon->MonteCarloStep(this, &Z, &interaction_type, Edep);
  }
  else {
    /*
    double E0 = Photon->E;
    Photon->MonteCarloStep(this, &Z, &interaction_type);
    if (Photon->w == 0) {
      *Edep = 0;
      return 0;
    }
    Photon->Scatter(Z, interaction_type);
    *Edep = E0 - Photon->E;
    // update absorption coefficients using the new energy value
    if (interaction_type != COHERENT) Comp->Mu(Photon->E);
    double Psurv=exp(-LinearAbsorption(Photon->x, Photon->uk));
    *Edep += Photon->E*(1.0 - Psurv);
    */
    double E0 = Photon->E;
    double w0 = Photon->w;
    Photon->MonteCarloStep(this, &Z, &interaction_type, Edep);
    Photon->w = w0;
    *Edep = E0;
  }
#ifdef TIME_PERF
  outph_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  //throw xrmc_exception(string("Breakpoint\n"));

  return 0;
}

//////////////////////////////////////////////////////////////////////
// generate an event with a photon forced to end on the point x1
// and evaluates the energy deposition in x1
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon_x1(photon *Photon, vect3 x1, double *mu_x1,
			  double *Edep, vect3 *prev_x)
{
  int Z, interaction_type;

#ifdef TIME_PERF
  clock_t cpu_time = clock();
#endif
  Out_Photon_x1(Photon, x1, prev_x);
#ifdef TIME_PERF
  outph_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
  cpu_time = clock();
#endif
  // it would be better to update photon position...
  Photon->EnergyDeposition(this, &Z, &interaction_type, mu_x1, Edep);
#ifdef TIME_PERF
  edep_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  //throw xrmc_exception(string("Breakpoint\n"));

  return 0;
}

//////////////////////////////////////////////////////////////////////
// weight the event with the survival probability
//////////////////////////////////////////////////////////////////////
int sample::PhotonSurvivalWeight(photon *Photon, double tmax)
{
  if (Photon->w>0) {
    Photon->w *= exp(-LinearAbsorption(Photon->x, Photon->uk, tmax));
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////
// generate an event up to the last interaction position
//////////////////////////////////////////////////////////////////////
int sample::Out_Photon(photon *Photon)
{
  int Z, interaction_type;

  if (PhotonNum[ScattOrderIdx]==0) {
    Photon->w = 0;
    return 0;
  }
  if (ScattOrderIdx == 0) { // transmission
    // ask the source to generate photon

#ifdef TIME_PERF
    clock_t cpu_time = clock();
#endif
    Source->Out_Photon(Photon);
#ifdef TIME_PERF
    soutph_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
    cpu_time = clock();
#endif
    Comp->Mu(Photon->E); // absorption coefficients at photon energy
#ifdef TIME_PERF
    mu_time0 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
  }
  else {
#ifdef TIME_PERF
    clock_t cpu_time = clock();
#endif
    PhotonHistory(Photon, Z, interaction_type, ScattOrderIdx);
#ifdef TIME_PERF
    phist_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
    if (Photon->w != 0) {
#ifdef TIME_PERF
      cpu_time = clock();
#endif
      // update the photon direction, polarization and energy
      Photon->Scatter(Z, interaction_type);
#ifdef TIME_PERF
      scatter_time += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
      // update absorption coefficients using the new energy value
      if (interaction_type != COHERENT) {
#ifdef TIME_PERF
	cpu_time = clock();
#endif
	Comp->Mu(Photon->E);
#ifdef TIME_PERF
	mu_time1 += (double)(clock() - cpu_time)/CLOCKS_PER_SEC;
#endif
      }
    }
  }
  // divide the event weight by the multiplicity
  Photon->w /= PhotonNum[ScattOrderIdx];

  return 0;
}

//////////////////////////////////////////////////////////////////////
// Clone method
//////////////////////////////////////////////////////////////////////
basesource *sample::Clone(string dev_name) {
	//cout << "Entering sample::Clone\n";	
	sample *clone = new sample(dev_name);

	*clone = *this;
	clone->compZ = compZ;
	clone->doppler_pz = doppler_pz;
	clone->rs = rs;

	clone->Path = Path->Clone();
	clone->PhotonNum = new int[ScattOrderNum];
	memcpy(clone->PhotonNum, PhotonNum, sizeof(int)*ScattOrderNum);  
	//clone Source
	clone->Source = Source->Clone(InputDeviceName[0]);
	clone->Geom3D = Geom3D->Clone(InputDeviceName[1]);
	clone->InputDeviceName[0] = InputDeviceName[0];
	clone->InputDeviceName[1] = InputDeviceName[1];
	clone->InputDeviceName[2] = InputDeviceName[2];
	//Comp is to be fetched from geom3d if possible
	if (InputDeviceName[2] == Geom3D->InputDeviceName[1])
		clone->Comp = clone->Geom3D->Comp;
	else
		clone->Comp = Comp->Clone(InputDeviceName[2]);

	return dynamic_cast<basesource*>(clone);
}

path *path::Clone() {
	//cout << "Entering path::Clone\n";
	path *clone = new path;

	clone->MaxNSteps = MaxNSteps;
	clone->t = new double[MaxNSteps];
	clone->Step = new double[MaxNSteps];
	clone->iPh0 = new int[MaxNSteps];
	clone->iPh1 = new int[MaxNSteps];
	clone->Mu = new double[MaxNSteps];
	clone->Delta = new double[MaxNSteps];
	clone->SumMuS = new double[MaxNSteps];
	clone->SumS = new double[MaxNSteps];

	return clone;
}

int sample::SetRng(randmt_t *rng)
{
  Rng = rng;
  Source->SetRng(rng);
  Comp->SetRng(rng);
  Path->Rng = rng;

  return 0;
}

int sample::PhCOn()
{
  PhCFlag=true;
  Source->PhCOn();

  return 0;
}

int sample::PhCOff()
{
  PhCFlag=false;
  Source->PhCOff();
  
  return 0;
}

int sample::Out_Phase_Photon_x1(photon *Photon, vect3 x1, double &muL,
				double &deltaL, vect3 *prev_x)
{
  //int Z, interaction_type;

  //double tmax=0;

  *prev_x = Source->X; //MUST use point source
  vect3 dummy_prev_x;
  // ask the source to generate photon directed torward the point x1
  Source->Out_Photon_x1(Photon, x1, &dummy_prev_x);
  vect3 vr = x1 - Source->X; // end position relative to photon start. position
                             //MUST use point source
  //tmax = vr.Mod(); // maximum intersection distance
  vr.Normalize(); // normalized direction
  Comp->Mu(Photon->E); // absorption coefficients at photon energy
  Comp->Delta(Photon->E); // delta coefficients at photon energy
  Photon->uk = vr; // update the photon direction FORSE NON SERVE!

  LinearMuDelta(Source->X, vr); //MUST use point source
  // in the future take care of tmax as in LinearAbsorption
  //if (Path->NSteps==1110) {
  //  cout << "E " << Photon->E << endl;
  //  cout << Source->X << endl;
  //  cout << vr << endl;
  //  cout << "NS " << Path->NSteps << endl;
  //  cout << Path->Step[0] << endl;
  //  cout << Path->MuL << endl;
  //}
  muL = deltaL = 0;
  if (Photon->w != 0) { //check that weight is not 0
    muL = Path->MuL;
    deltaL = Path->DeltaL;
  }
  Photon->x = x1; // update photon position
  
  return 0;
}

double sample::GetPhC_E0()
{
  return Source->GetPhC_E0();
}

vect3 sample::SourceX()
{
  return Source->X;
}
