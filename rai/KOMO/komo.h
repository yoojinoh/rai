/*  ------------------------------------------------------------------
    Copyright (c) 2011-2020 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#pragma once

#include "objective.h"
#include "skeletonSymbol.h"
#include "../Kin/kin.h"
#include "../Optim/optimization.h"
#include "../Optim/constrained.h"
#include "../Optim/MathematicalProgram.h"
#include "../Kin/switch.h"
#include "../Kin/featureSymbols.h"

//===========================================================================

namespace rai {
  struct FclInterface;
}

//===========================================================================

namespace rai {
enum KOMOsolver { KS_none=-1, KS_dense=0, KS_sparse, KS_banded, KS_sparseFactored, KS_NLopt, KS_Ipopt, KS_Ceres };
}

//===========================================================================


namespace rai {
  struct KOMO_Options {
    RAI_PARAM("KOMO/", int, verbose, 0)
    RAI_PARAM("KOMO/", int, animateOptimization, 0)
    RAI_PARAM("KOMO/", bool, mimicStable, false)
    RAI_PARAM("KOMO/", bool, useFCL, true)
  };
}//namespace

struct KOMO : NonCopyable {

  //-- the problem definition
  uint stepsPerPhase=0;        ///< time slices per phase
  uint T=0;                    ///< total number of time steps
  double tau=0.;               ///< real time duration of single step (used when evaluating feature space velocities/accelerations)
  uint k_order=0;              ///< the (Markov) order of the KOMO problem (default 2)
  ObjectiveL objectives;    ///< list of running objectives (each for a running interval of indexed time slices)
  rai::Array<ptr<GroundedObjective>> objs;  ///< list of grounded objective (each for only a single tuple of frames (not running intervals))
  rai::Array<ptr<rai::KinematicSwitch>> switches;  ///< list of kinematic switches along the motion -- only as record: they are applied immediately at addSwitch

  //-- internals
  rai::Configuration world;       ///< original configuration; which is the blueprint for all time-slice worlds (almost const: only makeConvexHulls modifies it)
  rai::Configuration pathConfig;  ///< configuration containing full path (T+k_order copies of world, with switches applied)
  uintA orgJointIndices;          ///< set of joint IDs (IDs of frames with dofs) of the original world
  FrameL timeSlices;              ///< the original timeSlices of the pathConfig (when switches add frames, pathConfig.frames might differ from timeSlices - otherwise not)
  bool computeCollisions;         ///< whether swift or fcl (collisions/proxies) is evaluated whenever new configurations are set (needed if features read proxy list)
  shared_ptr<rai::FclInterface> fcl;
  shared_ptr<SwiftInterface> swift;

  //-- optimizer
  rai::KOMOsolver solver=rai::KS_sparse;
  arr x, dual;                 ///< the primal and dual solution

  //-- options
  rai::KOMO_Options opt;

  //-- verbosity only: buffers of all feature values computed on last set_x
  double sos, eq, ineq;
  arr featureValues;           ///< storage of all features in all time slices
  arrA featureJacobians;       ///< storage of all features in all time slices
  ObjectiveTypeA featureTypes; ///< storage of all feature-types in all time slices
  StringA featureNames;
  double timeTotal=0.;           ///< measured run time
  double timeCollisions=0., timeKinematics=0., timeNewton=0., timeFeatures=0.;
  ofstream* logFile=0;

  KOMO();
  ~KOMO();

  //-- setup the problem
  void setModel(const rai::Configuration& C, bool _computeCollisions=true);
  void setTiming(double _phases=1., uint _stepsPerPhase=30, double durationPerPhase=5., uint _k_order=2);

  void clone(const KOMO& komo, bool deepCopyFeatures=true);

  //-- higher-level default setups
  void setIKOpt(); ///< setTiming(1., 1, 1., 1); and velocity objective

  //===========================================================================
  //
  // lowest level way to define objectives: basic methods to add any single objective or switch
  //

  /** THESE ARE THE TWO MOST IMPORTANT METHODS TO DEFINE A PROBLEM
   * they allow the user to add an objective, or a kinematic switch in the problem definition
   * Typically, the user does not call them directly, but uses the many methods below
   * Think of all of the below as examples for how to set arbirary objectives/switches yourself */
  ptr<struct Objective> addObjective(const arr& times, const ptr<Feature>& f, const StringA& frames,
                                     ObjectiveType type, const arr& scale=NoArr, const arr& target=NoArr, int order=-1, int deltaFromStep=0, int deltaToStep=0);
  ptr<struct Objective> addObjective(const arr& times, const FeatureSymbol& feat, const StringA& frames,
                                     ObjectiveType type, const arr& scale=NoArr, const arr& target=NoArr, int order=-1, int deltaFromStep=0, int deltaToStep=0) {
    return addObjective(times, symbols2feature(feat, frames, world),
                        NoStringA, type, scale, target, order, deltaFromStep, deltaToStep);
  }
  void clearObjectives(); ///< clear all objective

  void addContact_slide(double startTime, double endTime, const char* from, const char* to);
  void addContact_stick(double startTime, double endTime, const char* from, const char* to);
  void addContact_elasticBounce(double time, const char* from, const char* to, double elasticity=.8, double stickiness=0.);
  void addContact_ComplementarySlide(double startTime, double endTime, const char* from, const char* to);
  //  void addContact_Relaxed(double startTime, double endTime, const char *from, const char* to);

  void getBounds(arr& bounds_lo, arr& bounds_up);       ///< define the bounds (passed to the constrained optimization) based on the limit definitions of all DOFs

  //===========================================================================
  //
  // mid-level ways to define objectives: typically adding one specific objective
  //

  ptr<struct Objective> add_qControlObjective(const arr& times, uint order, double scale=1., const arr& target=NoArr, int deltaFromStep=0, int deltaToStep=0);
  void addQuaternionNorms(const arr& times=NoArr, double scale=3e0, bool hard=true);

  void add_collision(bool hardConstraint, double margin=.0, double prec=1e1);
  void add_jointLimits(bool hardConstraint, double margin=.05, double prec=1.);
  void setLiftDownUp(double time, const char* endeff, double timeToLift=.15);
  void setSlow(double startTime, double endTime, double prec=1e1, bool hardConstrained=false);
  void setSlowAround(double time, double delta, double prec=1e1, bool hardConstrained=false);

  //-- core kinematic switch symbols of skeletons
//protected:
  //low-level add dof switches
  void addSwitch(const arr& times, bool before, const ptr<rai::KinematicSwitch>& sw);
  ptr<rai::KinematicSwitch> addSwitch(const arr& times, bool before, bool stable, rai::JointType type, rai::SwitchInitializationType init,
                 const char* ref1, const char* ref2,
                 const rai::Transformation& jFrom=NoTransformation, const rai::Transformation& jTo=NoTransformation);
public:
  //add a mode switch: both, the low-level dof switches and corresponding constraints of consistency
  void addModeSwitch(const arr& times, rai::SkeletonSymbol newMode, const StringA& frames, bool firstSwitch);

  //advanced:
  void setPairedTimes();
  void addTimeOptimization();

  //===========================================================================
  //
  // optimizing, getting results, and verbosity
  //

  //-- setting individual time slices
  void setConfiguration_qAll(int t, const arr& q); ///< t<0 allows to set the prefix configurations; while 0 <= t < T allows to set all other initial configurations
  void setConfiguration_qOrg(int t, const arr& q); ///< set only those DOFs that were defined in the original world (excluding extra DOFs from switches)
  void setConfiguration_X(int t, const arr& X); ///< t<0 allows to set the prefix configurations; while 0 <= t < T allows to set all other initial configurations
  void initWithConstant(const arr& q); ///< set all configurations EXCEPT the prefix to a particular state
  void initWithWaypoints(const arrA& waypoints, uint waypointStepsPerPhase=1); ///< set all configurations (EXCEPT prefix) to interpolate given waypoints
  void updateRootObjects(const rai::Configuration& C);
  void updateAndShiftPrefix(const rai::Configuration& C);


  //-- optimization
  void optimize(double addInitializationNoise=.01, const rai::OptOptions options=NOOPT);  ///< run the solver (same as run_prepare(); run(); )
  void reset();                                      ///< reset the dual variables and feature value buffers (always needed when adding/changing objectives before continuing an optimization)

  //advanced
  void run_prepare(double addInitializationNoise);   ///< ensure the configurations are setup, decision variable is initialized, and noise added (if >0)
  void run(rai::OptOptions options=NOOPT);          ///< run the solver iterations (configurations and decision variable needs to be setup before)
  void setSpline(uint splineT);      ///< optimize B-spline nodes instead of the path; splineT specifies the time steps per node

  //-- reading results
  arr getConfiguration_qAll(int t);  ///< get all DOFs
  arr getConfiguration_qOrg(int t);  ///< get only those DOFs that were defined in the original world (excluding extra DOFs from switches)
  arr getConfiguration_X(int t);     ///< get frame path for selected frames

  arrA getPath_qAll();                            ///< get the DOFs (of potentially varying dimensionality) for each configuration
  arr getPath_qOrg();      ///< get joint path, optionally for selected joints
  arr getPath_X();     ///< get frame path, optionally for selected frames

  arr getPath_tau();
  arr getPath_times();
  arr getPath_energies();

  arr getActiveConstraintJacobian();

  void reportProblem(ostream& os=std::cout);
  rai::Graph getReport(bool gnuplt=false, int reportFeatures=0, ostream& featuresOs=std::cout); ///< return a 'dictionary' summarizing the optimization results (optional: gnuplot objective costs; output detailed cost features per time slice)
  rai::Graph getProblemGraph(bool includeValues, bool includeSolution=true);
  double getConstraintViolations();
  double getCosts();
  StringA getCollisionPairs(double belowMargin=.01); ///< report the proxies (collisions) for each time slice

  void checkGradients();          ///< checks all gradients numerically

  int view(bool pause=false, const char* txt=nullptr);
  int view_play(bool pause=false, double delay=.2, const char* saveVideoPath=nullptr);

  void plotTrajectory();
  void plotPhaseTrajectory();

  //===========================================================================
  //
  // internal (kind of private)
  //

  void selectJointsBySubtrees(const StringA& roots, const arr& times= {}, bool notThose=false);
  void setupPathConfig();
  void checkBounds(const arr& x);
  void applySwitch(const rai::KinematicSwitch& sw);
  void retrospectApplySwitches();
  void retrospectChangeJointType(int startStep, int endStep, uint frameID, rai::JointType newJointType);
  void set_x(const arr& x, const uintA& selectedConfigurationsOnly=NoUintA);            ///< set the state trajectory of all configurations


  //===========================================================================
  //
  // MP transcriptions
  //

  shared_ptr<MathematicalProgram> mp_SparseNonFactored();
  shared_ptr<MathematicalProgram_Factored> mp_Factored();


  //===========================================================================
  //
  // deprecated
  //

  void addSquaredQuaternionNorms(const arr& times=NoArr, double scale=3e0){ DEPR; addQuaternionNorms(times, scale); }

  bool displayTrajectory(double delay=1., bool watch=true, bool overlayPaths=true, const char* saveVideoPath=nullptr, const char* addText=nullptr){
    DEPR; return view_play(watch, delay, saveVideoPath);  }
  bool displayPath(const char* txt, bool watch=true, bool full=true){
    DEPR; return view(watch, txt); }
  rai::Camera& displayCamera();

  void add_StableRelativePose(const std::vector<int>& confs, const char* gripper, const char* object) {
    DEPR;
    for(uint i=1; i<confs.size(); i++)
      addObjective(ARR(confs[0], confs[i]), FS_poseRel, {gripper, object}, OT_eq);
    world.makeObjectsFree({object});
  }
  void add_StablePose(const std::vector<int>& confs, const char* object) {
    DEPR;
    for(uint i=1; i<confs.size(); i++)
      addObjective(ARR(confs[0], confs[i]), FS_pose, {object}, OT_eq);
    world.makeObjectsFree({object});
  }
  void add_grasp(int conf, const char* gripper, const char* object) {
    DEPR;
    addObjective(ARR(conf), FS_distance, {gripper, object}, OT_eq);
  }
  void add_place(int conf, const char* object, const char* table) {
    DEPR;
    addObjective(ARR(conf), FS_aboveBox, {table, object}, OT_ineq);
    addObjective(ARR(conf), FS_standingAbove, {table, object}, OT_eq);
    addObjective(ARR(conf), FS_vectorZ, {object}, OT_sos, {}, {0., 0., 1.});
  }
  void add_resting(int conf1, int conf2, const char* object) {
    DEPR;
    addObjective(ARR(conf1, conf2), FS_pose, {object}, OT_eq);
  }
  void add_restingRelative(int conf1, int conf2, const char* object, const char* tableOrGripper) {
    DEPR;
    addObjective(ARR(conf1, conf2), FS_poseRel, {tableOrGripper, object}, OT_eq);
  }
  void activateCollisions(const char* s1, const char* s2){ DEPR; HALT("see komo-21-03-06"); }
  void deactivateCollisions(const char* s1, const char* s2);
  arr getFrameStateX(int t){ DEPR; return getConfiguration_X(t); }
  arr getPath_qAll(int t){ DEPR; return getConfiguration_qOrg(t); }
  arr getConfiguration_q(int t) { DEPR; return getConfiguration_qAll(t); }
  arr getPath_qOrg(uintA joints, const bool activesOnly){ DEPR; return getPath_qOrg(); }

private:
  void _addObjective(const std::shared_ptr<Objective>& ob, const intA& timeSlices);
};

