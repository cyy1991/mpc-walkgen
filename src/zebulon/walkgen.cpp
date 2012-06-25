#include "walkgen.h"

#include "../common/qp-solver.h"
#include "qp-generator.h"
#include "qp-generator-orientation.h"
#include "rigid-body-system.h"
#include "../common/interpolation.h"

#include <iostream>
#include <Eigen/Dense>

using namespace MPCWalkgen;
using namespace Zebulon;
using namespace Eigen;


MPCWalkgen::Zebulon::WalkgenAbstract* MPCWalkgen::Zebulon::createWalkgen(MPCWalkgen::QPSolverType solvertype) {
  MPCWalkgen::Zebulon::WalkgenAbstract* zmpVra = new MPCWalkgen::Zebulon::Walkgen(solvertype);
  return zmpVra;
}


// Implementation of the private interface
Walkgen::Walkgen(::MPCWalkgen::QPSolverType solvertype)
  : WalkgenAbstract()
  ,mpcData_()
  ,solver_(0x0)
  ,solverOrientation_(0x0)
  ,generator_(0x0)
  ,generatorOrientation_(0x0)
  ,interpolation_(0x0)
  ,robot_(0x0)
  ,solution_()
  ,velRef_()
  ,upperTimeLimitToUpdate_(0)
  ,upperTimeLimitToFeedback_(0)
  ,initAlreadyCalled_(false)
{

  solver_ = createQPSolver(qpSolverType_,
          4*mpcData_.nbSamplesQP, 9*mpcData_.nbSamplesQP,
          4*mpcData_.nbSamplesQP, 9*mpcData_.nbSamplesQP);
  solverOrientation_ = createQPSolver(qpSolverType_,
          mpcData_.nbSamplesQP, 2*mpcData_.nbSamplesQP,
          mpcData_.nbSamplesQP, 2*mpcData_.nbSamplesQP);
  interpolation_ = new Interpolation();
  robot_ = new RigidBodySystem(&mpcData_, interpolation_);
  generator_= new QPGenerator(solver_, &velRef_, &posRef_, robot_, &mpcData_);
  generatorOrientation_= new QPGeneratorOrientation(solverOrientation_, &velRef_, &posRef_, robot_, &mpcData_);

}


Walkgen::~Walkgen(){
  if (solverOrientation_ != 0x0)
    delete solverOrientation_;

  if (solver_ != 0x0)
    delete solver_;

  if (generator_ != 0x0)
    delete generator_;

  if (generatorOrientation_ != 0x0)
    delete generatorOrientation_;

  if (robot_ != 0x0)
    delete robot_;

  if (interpolation_ != 0x0)
    delete interpolation_;

}

  if (!initAlreadyCalled_){
    init();
    return;
  }

  // Modify dynamic matrices wich are used everywhere in the problem.
  // So, we must to recall init()
    init();
    return;
  }

  }

  }

  }
  }
void Walkgen::robotData(const RobotData &robotData){
  RobotData tmpRobotData = robotData_;
  robotData_ = robotData;
  if (!initAlreadyCalled_){
    init();
    return;
  }

  // Modify dynamic matrices wich are used everywhere in the problem.
  // So, we must to recall init()
  if (fabs(robotData.CoMHeight-tmpRobotData.CoMHeight)>EPSILON){
    init();
    return;
  }
}

void Walkgen::init(const RobotData &robotData, const MPCData &mpcData){
  mpcData_ = mpcData;
  robotData_ = robotData;
  init();
}

void Walkgen::init(const MPCData &mpcData){
  mpcData_ = mpcData;
  init();
}

void Walkgen::init(const RobotData &robotData){
  robotData_ = robotData;
  init();
}

void Walkgen::init() {

  robot_->init(robotData_);

  //Check if sampling periods are defined correctly
  assert(mpcData_.actuationSamplingPeriod > 0);
  assert(mpcData_.MPCSamplingPeriod >= mpcData_.actuationSamplingPeriod);
  assert(mpcData_.QPSamplingPeriod >= mpcData_.MPCSamplingPeriod);

  robot_->computeDynamics();

  generator_->precomputeObjective();
  generatorOrientation_->precomputeObjective();

  BodyState state;
  robot_->body(BASE)->state(state);

  state.x[0] = 0;
  state.y[0] = 0;
  state.z[0] = robotData_.CoMHeight;
  robot_->body(COM)->state(state);

  currentRealTime_ = 0.0;
  currentTime_ = 0.0;
  upperTimeLimitToUpdate_ = 0.0;
  upperTimeLimitToFeedback_ = 0.0;

  mpcData_.ponderation.activePonderation = 0;

  velRef_.resize(mpcData_.nbSamplesQP);
  newVelRef_.resize(mpcData_.nbSamplesQP);

  posRef_.resize(mpcData_.nbSamplesQP);
  newPosRef_.resize(mpcData_.nbSamplesQP);

  initAlreadyCalled_ = true;
}

const MPCSolution & Walkgen::online(bool previewBodiesNextState){
  currentRealTime_ += mpcData_.MPCSamplingPeriod;
  return online(currentRealTime_, previewBodiesNextState);
}

const MPCSolution & Walkgen::online(double time, bool previewBodiesNextState){
  currentRealTime_ = time;
  solution_.mpcSolution.newTraj = false;
  if(time  > upperTimeLimitToUpdate_+EPSILON){
      upperTimeLimitToUpdate_ += mpcData_.QPSamplingPeriod;
      currentTime_ = time;
    }

  if (time  > upperTimeLimitToFeedback_ + EPSILON) {

      solver_->reset();
      solverOrientation_->reset();
      solution_.mpcSolution.newTraj = true;
      velRef_ = newVelRef_;
      posRef_ = newPosRef_;

      upperTimeLimitToFeedback_ += mpcData_.MPCSamplingPeriod;

      generatorOrientation_->computeReferenceVector();
      generatorOrientation_->buildObjective();
      generatorOrientation_->buildConstraints();
      generatorOrientation_->computeWarmStart(solution_);

      solverOrientation_->solve(solution_.qpSolutionOrientation,
                                solution_.constraintsOrientation, solution_.initialSolutionOrientation,
                                solution_.initialConstraintsOrientation, solution_.useWarmStart);

      generator_->computeReferenceVector(solution_);
      generator_->buildObjective();
      generator_->buildConstraints();
      generator_->computeWarmStart(solution_);

      solver_->solve(solution_.qpSolution, solution_.constraints,
                     solution_.initialSolution, solution_.initialConstraints, solution_.useWarmStart);

      robot_->interpolateBodies(solution_, time, velRef_);

      if (previewBodiesNextState){
          robot_->updateBodyState(solution_);
        }

    }

  return solution_.mpcSolution;
}

void Walkgen::velReferenceInLocalFrame(double dx, double dy, double dyaw){
  newVelRef_.local.x.fill(dx);
  newVelRef_.local.y.fill(dy);
  newVelRef_.local.yaw.fill(dyaw);
}

void Walkgen::velReferenceInLocalFrame(Eigen::VectorXd dx, Eigen::VectorXd dy, Eigen::VectorXd dyaw){
  newVelRef_.local.x=dx;
  newVelRef_.local.y=dy;
  newVelRef_.local.yaw=dyaw;
}

void Walkgen::velReferenceInGlobalFrame(double dx, double dy, double dyaw){
  newVelRef_.global.x.fill(dx);
  newVelRef_.global.y.fill(dy);
  newVelRef_.global.yaw.fill(dyaw);
}

void Walkgen::velReferenceInGlobalFrame(Eigen::VectorXd dx, Eigen::VectorXd dy, Eigen::VectorXd dyaw){
  newVelRef_.global.x=dx;
  newVelRef_.global.y=dy;
  newVelRef_.global.yaw=dyaw;
}

void Walkgen::posReferenceInGlobalFrame(double dx, double dy, double dyaw){
  newPosRef_.global.x.fill(dx);
  newPosRef_.global.y.fill(dy);
  newPosRef_.global.yaw.fill(dyaw);
}

void Walkgen::posReferenceInGlobalFrame(Eigen::VectorXd dx, Eigen::VectorXd dy, Eigen::VectorXd dyaw){
  newPosRef_.global.x=dx;
  newPosRef_.global.y=dy;
  newPosRef_.global.yaw=dyaw;
}

const BodyState & Walkgen::bodyState(BodyType body)const{
  return robot_->body(body)->state();
}
void Walkgen::bodyState(BodyType body, const BodyState & state){
  robot_->body(body)->state(state);
}

