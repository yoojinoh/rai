#include <KOMO/komo.h>
#include <Kin/TM_default.h>
#include <Kin/F_collisions.h>
#include <Kin/viewer.h>
#include <Kin/F_pose.h>
#include <Optim/solver.h>

//===========================================================================

void TEST(Easy){
  rai::Configuration C("arm.g");
  cout <<"configuration space dim=" <<C.getJointStateDimension() <<endl;
  
  KOMO komo;
  komo.setModel(C);
  komo.setTiming(1., 100, 5., 2);
  komo.add_qControlObjective({}, 2, 1.);
  komo.addQuaternionNorms({}, 1., false);

  //-- set a time optim objective
//  komo.addObjective({}, make_shared<TM_Time>(), OT_sos, {1e2}, {}, 1); //smooth time evolution
//  komo.addObjective({}, make_shared<TM_Time>(), OT_sos, {1e1}, {komo.tau}, 0); //prior on timing

  komo.addObjective({1.}, FS_positionDiff, {"endeff", "target"}, OT_eq, {1e2});
  komo.addObjective({1.}, FS_qItself, {}, OT_eq, {1e2}, {}, 1);
  komo.addObjective({}, FS_accumulatedCollisions, {}, OT_eq, {1e0});

  komo.reportProblem();

//  komo.setSpline(5);
  komo.optimize();
  cout <<"TIME OPTIM: total=" <<sum(komo.getPath_times()) <<komo.getPath_times() <<endl;
  komo.plotTrajectory();
//  komo.reportProxies();

  komo.view(true, "result");
  while(komo.view_play(true));
}

//===========================================================================

void TEST(Align){
  rai::Configuration C("arm.g");
  cout <<"configuration space dim=" <<C.getJointStateDimension() <<endl;

  KOMO komo;
//  komo.solver = rai::KS_sparseStructured; //set via rai.cfg!!
//  komo.verbose=1; //set via rai.cfg!!
  komo.setModel(C);
  komo.setTiming(1., 100, 5., 2);

  komo.add_qControlObjective({}, 2, 1.);

  komo.addObjective({1.}, FS_positionDiff, {"endeff", "target"}, OT_eq, {1e1});
  komo.addObjective({1.}, FS_quaternionDiff, {"endeff", "target"}, OT_eq, {1e1});
  komo.addObjective({1.}, FS_qItself, {}, OT_eq, {1e1}, {}, 1);
  komo.addObjective({}, FS_accumulatedCollisions, {}, OT_eq, {1.});

  komo.optimize();
//  komo.checkGradients();

//  komo.plotTrajectory();
  komo.view(true, "result");
  while(komo.view_play(true));

//  komo.pathConfig.setJointState(komo.x);
//  V.setConfiguration(komo.pathConfig, "path", true);
}

//===========================================================================

struct MyFeature : Feature {
  virtual void phi2(arr& y, arr& J, const FrameL& F){
    CHECK_EQ(order, 1, "");

    auto V = F_PositionDiff().setOrder(1).eval(F);

    auto C = F_PairCollision(F_PairCollision::_normal, false)
             .eval(F[1]);

    auto D = F_PairCollision(F_PairCollision::_negScalar, false)
             .eval(F[1]);

    //penalizing velocity whenever close
    double range=.2;
    if(-D.scalar() > range){
      y = zeros(3);
      if(!!J) J = zeros(3, V.J().d1);
      return;
    }

    double weight = 1. + D.scalar()/range;
    double normalWeight = 1.;

    arr CJ = C.J_reset();
    arr DJ = D.J_reset();
    arr VJ = V.J_reset();

    arr P = eye(3) + normalWeight*(C*~C);
    y = weight * P * V;
    if(!!J){
      J = weight * P * VJ;
      J += P * V.reshape(3,1) * (1./range)*DJ;
      J += (weight * 2. * normalWeight * scalarProduct(C,V)) * CJ;
    }

#if 0
    //penalizing normal velocity
    double normalVel = scalarProduct(V, C);
    if(normalVel>0.){
      y = 0.;
      if(!!J) J = zeros(1, V.J().d1);
      return;
    }

    double scale = 3.;
    double weight = ::exp(scale * D.scalar());
    weight = 1.; scale=0.;

    y.resize(1);
    y(0) = weight * normalVel;
    if(!!J){
      J = weight * ( ~V * C.J() + ~C * V.J() );
      J += (normalVel * weight * scale) * D.J();
    }

#if 0
    normalVel += 1.;

    y = D / normalVel;

    if(!!J){
      J = D.J() / normalVel;
      J += (-D.scalar() / (normalVel*normalVel)) * ( ~V * C.J() + ~C * V.J() );
    }
#endif
#endif

  }

  virtual uint dim_phi2(const FrameL& F) {  return 3;  }
};

void TEST(Thin){
  rai::Configuration C("thin.g");
  cout <<"configuration space dim=" <<C.getJointStateDimension() <<endl;

  KOMO komo;
  komo.setModel(C);
  komo.setTiming(1., 60, 5., 2);
  komo.add_qControlObjective({}, 2, 1.);

  //-- set a time optim objective
//  komo.addObjective({}, make_shared<TM_Time>(), OT_sos, {1e2}, {}, 1); //smooth time evolution
//  komo.addObjective({}, make_shared<TM_Time>(), OT_sos, {1e1}, {komo.tau}, 0); //prior on timing

  komo.addObjective({1.}, FS_positionDiff, {"ball", "target"}, OT_eq, {1e1});
  komo.addObjective({1.}, FS_qItself, {}, OT_eq, {1e1}, {}, 1);
  komo.addObjective({}, FS_distance, {"wall", "ball"}, OT_ineq, {1.});

  komo.addObjective({}, make_shared<MyFeature>(), {"ball", "wall"}, OT_sos, {1e1}, {}, 1);

  komo.reportProblem();

  komo.opt.animateOptimization=1;
  //  komo.setSpline(5);
  komo.optimize(1e-2);
  komo.plotTrajectory();
//  komo.reportProxies();
//  komo.checkGradients();

  komo.view(true, "result");
  while(komo.view_play(true));
}

//===========================================================================

void TEST(PR2){
  rai::Configuration C("model.g");
  C.optimizeTree(true);
  cout <<"configuration space dim=" <<C.getJointStateDimension() <<endl;
  double rand = rai::getParameter<double>("KOMO/moveTo/randomizeInitialPose", .0);
  if(rand){
    rnd.seed(rai::getParameter<uint>("rndSeed", 0));
    rndGauss(C.q,rand,true);
    C.setJointState(C.q);
  }

  KOMO komo;
//  komo.logFile = new ofstream("z.dat");
//  komo.denseOptimization=true;
//  komo.sparseOptimization=true;
  komo.setModel(C);
  komo.setTiming(1., 100, 10., 2);
  komo.add_qControlObjective({}, 2, 1.);
  komo.addObjective({1.}, FS_positionDiff, {"endeff", "target"}, OT_eq, {1e1});
  komo.addObjective({.98,1.}, FS_qItself, {}, OT_sos, {1e1}, {}, 1);
  komo.addObjective({}, FS_accumulatedCollisions, {}, OT_eq, {1.});

//  komo.setSpline(10);
  komo.optimize();
  komo.plotTrajectory();
//  komo.checkGradients();
  komo.view(true, "result");
  while(komo.view_play(true));
}

//===========================================================================

// void TEST(FinalPosePR2){
//   rai::Configuration K("model.g");
//   K.pruneRigidJoints();
//   K.optimizeTree();
//   makeConvexHulls(K.frames);
//   cout <<"configuration space dim=" <<K.getJointStateDimension() <<endl;
//   arr x = finalPoseTo(K, *K.getFrameByName("endeff"), *K.getFrameByName("target"));
//   K.setJointState(x.reshape(x.N));
//   K.watch(true);
// }

//===========================================================================

int main(int argc,char** argv){
  rai::initCmdLine(argc,argv);

//  rnd.clockSeed();

  testEasy();
  testAlign();
  testThin();
  testPR2();

  return 0;
}

