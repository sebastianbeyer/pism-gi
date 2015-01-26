// Copyright (C) 2011, 2012, 2014, 2015 David Maxwell
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


#ifndef _SNESPROBLEM_H_
#define _SNESPROBLEM_H_

#include "IceGrid.hh"           // inline implementation in the header uses IceGrid
#include "iceModelVec.hh"       // to get Vector2

namespace pism {

template<int DOF, class U> class SNESProblem {
public:
  SNESProblem(const IceGrid &g);

  virtual ~SNESProblem();

  virtual PetscErrorCode solve();

  virtual const std::string& name();

  virtual Vec solution()
  {
    return m_X;
  }

protected:

  typedef PetscErrorCode (*DMDASNESJacobianLocal)(DMDALocalInfo*,void*,Mat,Mat,MatStructure*,void*);
  typedef PetscErrorCode (*DMDASNESFunctionLocal)(DMDALocalInfo*,void*,void*,void*);

  virtual PetscErrorCode initialize();

  virtual PetscErrorCode setFromOptions();

  virtual PetscErrorCode finalize();

  virtual PetscErrorCode compute_local_function(DMDALocalInfo *info, const U **xg, U **yg) = 0;

  virtual PetscErrorCode compute_local_jacobian(DMDALocalInfo *info, const U **x,  Mat B) = 0;

  const IceGrid &m_grid;

  petsc::Vec   m_X;
  SNES         m_snes;
  PISMDM::Ptr  m_DA;

private:

  struct SNESProblemCallbackData {
    DM           da;
    SNESProblem<DOF,U> *solver;
  };

  SNESProblemCallbackData m_callbackData;

  static PetscErrorCode LocalFunction(DMDALocalInfo *info, const U **x, U **f, SNESProblemCallbackData *);
  static PetscErrorCode LocalJacobian(DMDALocalInfo *info, const U **x, Mat B, SNESProblemCallbackData *);

};

typedef SNESProblem<1,double> SNESScalarProblem;
typedef SNESProblem<2,Vector2> SNESVectorProblem;



template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::LocalFunction(DMDALocalInfo *info,
                                                 const U **x, U **f,
                                                 SNESProblem<DOF,U>::SNESProblemCallbackData *cb)
{
  return cb->solver->compute_local_function(info,x,f);
}

template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::LocalJacobian(DMDALocalInfo *info,
                                                 const U **x, Mat J,
                                                 SNESProblem<DOF,U>::SNESProblemCallbackData *cb)
{
  return cb->solver->compute_local_jacobian(info,x,J);
}


template<int DOF, class U>
SNESProblem<DOF,U>::SNESProblem(const IceGrid &g) :
  m_grid(g)
{
  PetscErrorCode ierr;
  ierr = setFromOptions(); CHKERRABORT(m_grid.com,ierr);
  ierr = initialize();     CHKERRABORT(m_grid.com,ierr);
}
template<int DOF, class U>
SNESProblem<DOF,U>::~SNESProblem()
{
  PetscErrorCode ierr = finalize();
  CHKERRABORT(m_grid.com,ierr);
}

template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::setFromOptions()
{
  return 0;
}

template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::initialize()
{
  PetscErrorCode ierr;

  int stencil_width=1;
  m_DA = m_grid.get_dm(DOF, stencil_width);

  ierr = DMCreateGlobalVector(*m_DA, m_X.rawptr()); CHKERRQ(ierr);

  ierr = SNESCreate(m_grid.com, &m_snes); CHKERRQ(ierr);

  // Set the SNES callbacks to call into our compute_local_function and compute_local_jacobian
  // methods via SSAFEFunction and SSAFEJ
  m_callbackData.da = *m_DA;
  m_callbackData.solver = this;
#if PETSC_VERSION_LT(3,5,0)
  ierr = DMDASNESSetFunctionLocal(*m_DA,INSERT_VALUES,
                                  (DMDASNESFunctionLocal)SNESProblem<DOF,U>::LocalFunction,
                                  &m_callbackData); CHKERRQ(ierr);
  ierr = DMDASNESSetJacobianLocal(*m_DA,(DMDASNESJacobianLocal)SNESProblem<DOF,U>::LocalJacobian,
                                  &m_callbackData); CHKERRQ(ierr);
#else
  ierr = DMDASNESSetFunctionLocal(*m_DA,INSERT_VALUES,
                                  (DMDASNESFunction)SNESProblem<DOF,U>::LocalFunction,
                                  &m_callbackData); CHKERRQ(ierr);
  ierr = DMDASNESSetJacobianLocal(*m_DA,(DMDASNESJacobian)SNESProblem<DOF,U>::LocalJacobian,
                                  &m_callbackData); CHKERRQ(ierr);
#endif

  ierr = DMSetMatType(*m_DA, "baij"); CHKERRQ(ierr);
  ierr = DMSetApplicationContext(*m_DA, &m_callbackData); CHKERRQ(ierr);

  ierr = SNESSetDM(m_snes, *m_DA); CHKERRQ(ierr);

  ierr = SNESSetFromOptions(m_snes); CHKERRQ(ierr);

  return 0;
}

//! Undo the allocations of SSAFEM::allocate_fem; called by the destructor.
template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::finalize() {
  PetscErrorCode ierr;

  ierr = SNESDestroy(&m_snes); CHKERRQ(ierr);

  return 0;
}

template<int DOF, class U>
const std::string& SNESProblem<DOF,U>::name()
{
  return "UnnamedProblem";
}

template<int DOF, class U>
PetscErrorCode SNESProblem<DOF,U>::solve()
{
  PetscErrorCode ierr;

  // Solve:
  ierr = SNESSolve(m_snes,NULL,m_X); CHKERRQ(ierr);

  // See if it worked.
  SNESConvergedReason reason;
  ierr = SNESGetConvergedReason(m_snes, &reason); CHKERRQ(ierr);
  if (reason < 0) {
    throw RuntimeError::formatted("SNESProblem %s solve failed to converge (SNES reason %s)",
                                  name().c_str(), SNESConvergedReasons[reason]);
  }

  verbPrintf(1,m_grid.com,"SNESProblem %s converged (SNES reason %s)\n",
             name().c_str(), SNESConvergedReasons[reason]);

  return 0;
}


} // end of namespace pism

#endif
