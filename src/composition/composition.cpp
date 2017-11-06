/*
Copyright (C) 2013 Bruno Golosio

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
//     composition.cpp           //
//        31/10/2017             //
//   author : Bruno Golosio      //
///////////////////////////////////
// Methods of the classes composition and phase
//
#include "xrmc_composition.h"
#include "xraylib.h"
#include "xrmc_algo.h"
#include <iostream>
#include <cstring>

using namespace std;
using namespace xrmc_algo;

const double phase::KD=4.15179082788e-4; // constant for computing Delta

 // destructor
composition::~composition() {
}

// constructor
composition::composition(string dev_name) {
  Runnable = false;
  NInputDevices=0;
  SetDevice(dev_name, std::string("composition"));
}

// Evaluates the absorption coefficient Mu of each phase at energy E
int composition::Mu(double E)
{
  for (unsigned int i=0; i<Ph.size(); i++) {
    Ph[i].Mu(E);
  }

  return 0;
}

// Evaluates the coefficient delta of each phase at energy E
// 1-delta is the real part of the refractive index
int composition::Delta(double E)
{
  for (unsigned int i=0; i<Ph.size(); i++) {
    Ph[i].Delta(E);
  }

  return 0;
}

// Evaluates the absorption coefficient Mu of the phase at energy E
int phase::Mu(double E)
{
  double mu = 0;
  for (int i=0; i<NElem(); i++) { // loop on all elements of the phase
    MuAtom[i] = CS_Total(Z[i], E); // total cross section  for atomic num. Z
    mu += W[i]*MuAtom[i]; // sum of C.S. weighted by the weight fractions
  }
  LastMu = mu*Rho; // linear absorption coefficient (Rho is the mass density)

  return 0;
}

// Evaluates the delta coefficient of the phase at energy E
int phase::Delta(double E)
{
  double delta = 0;
  for (int i=0; i<NElem(); i++) {
    // for details about the calculation see references in the documentation
    double num = W[i]*KD*(Z[i]+Fi(Z[i],E));
    double denom = AtomicWeight(Z[i])*E*E;
    // check that the denominator is not much smaller than numerator
    if (num+denom == num) delta = 0;
    else delta += num/denom;
  }
  LastDelta = delta*Rho; // delta coefficient (Rho is the mass density)

  return 0;
}

// extract the atomic species with which the interaction will occur
// Zelem: atomic number; mu_atom: total cross section for this element
int phase::AtomType(int *Zelem, double *mu_atom)
{
  // check that Rho is not 0 and that it is not much smaller than numerator
  if (Rho==0 || LastMu+Rho==LastMu) return -1;

  double mu_compound = LastMu / Rho; // absorption coefficient of the phase
  double R = Rnd_r(Rng)*mu_compound; // random num. between 0 and mu_compound

  double sum = 0;
  int i = 0;
  // extract the index of the element with which the interaction will occur
  while (sum <= R) { // stop when the cumulative distribution is >= R
    sum += W[i]*MuAtom[i];
    i++;
  }
  if (i > NElem()) i = NElem();
  i--;
  *Zelem = Z[i]; // atomic number of the element
  *mu_atom = MuAtom[i]; // total cross section for the element

  return 0;
}

// extract the index of the phase with which the interaction will occur
int phase::PhaseType(randmt_t *rng, composition *Comp, int NPh, int *iPh,
		     double *Fact)
{
  double mu_tot=0;

  for (int i=0; i<NPh; i++) {
    mu_tot += Fact[i]*Comp->Ph[iPh[i]].LastMu;
  }
  double R = Rnd_r(rng)*mu_tot; // random num. between 0 and mu_tot


  double sum = 0;
  int i = 0;
  // extract the index of the phase with which the interaction will occur
  while (sum <= R) { // stop when the cumulative distribution is >= R
    sum += Fact[i]*Comp->Ph[iPh[i]].LastMu;
    i++;
  }
  if (i > NPh) i = NPh;
  i--;

  return iPh[i];
}

composition *composition::Clone(string dev_name) {
	//cout << "Entering composition::Clone\n";
	composition *clone = new composition(dev_name);

	*clone = *this;

	return clone;
}

int composition::SetRng(randmt_t *rng)
{
  Rng = rng;

  for (unsigned int i=0; i<Ph.size(); i++) {
    Ph[i].Rng = rng;
  }

  return 0;
}

int composition::ReduceMap(vector<string> used_phases)
{
  vector<phase> Ph_new;
  phase_map PhaseMap_new;
  phase_map::iterator it;

  vector<material> Mater_new = Mater;
  phase_map MaterMap_new;

  for (unsigned int iph=0 ; iph<used_phases.size() ; iph++) {
    it = PhaseMap.find(used_phases.at(iph));
    if (it==PhaseMap.end()) {
      throw xrmc_exception(string("Phase ") + used_phases.at(iph)
                           + " not found in phase map\n");
    }
    Ph_new.push_back(Ph[it->second]);
    PhaseMap_new.insert(phase_map_pair(used_phases.at(iph), iph));
    //int iph_old = it->first;
  }

  Ph = Ph_new;
  PhaseMap = PhaseMap_new;

  return 0;
}

int composition::ReduceMaterMap(vector<string> used_mater)
{
  vector<phase> Ph_new;
  phase_map PhaseMap_new;
  phase_map::iterator it;

  vector<int> ph_old_index;

  vector<material> Mater_new;
  phase_map MaterMap_new;

  for (unsigned int imat=0 ; imat<used_mater.size() ; imat++) {
    it = MaterMap.find(used_mater[imat]);
    if (it==MaterMap.end()) {
      throw xrmc_exception(string("Material ") + used_mater[imat]
			   + " not found in material map\n");
    }
    Mater_new.push_back(Mater[it->second]);
    MaterMap_new.insert(phase_map_pair(used_mater[imat], imat));
    for (int i_comp=0 ; i_comp<Mater_new[imat].NPh(); i_comp++) {
      int i_ph_old = Mater_new[imat].iPh[i_comp];

      string comp_name="";
      for (phase_map::iterator it=PhaseMap.begin();
	   it!=PhaseMap.end(); ++it) {
	if (it->second==i_ph_old) {
	  comp_name = it->first;
	  break;
	}
      }
      if (comp_name=="") {
	throw xrmc_exception("Compound index not found in composition map\n");
      }

      phase_map::iterator it = PhaseMap_new.find(comp_name);
      if (it==PhaseMap_new.end()) { // phase not yet remapped
	int i_ph_new = Ph_new.size();
	Ph_new.push_back(Ph[i_ph_old]);
	PhaseMap_new.insert(phase_map_pair(comp_name, i_ph_new));
	Mater_new[imat].iPh[i_comp] = i_ph_new;
      }
      else {
	Mater_new[imat].iPh[i_comp] = it->second;
      }
    }
  }

  Mater = Mater_new;
  MaterMap = MaterMap_new;

  Ph = Ph_new;
  PhaseMap = PhaseMap_new;

  return 0;
}
