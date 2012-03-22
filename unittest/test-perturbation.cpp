
/*
 */



#include <mpc-walkgen/sharedpgtypes.h>
#include <mpc-walkgen/walkgen.h>
#include <mpc-walkgen/mpc-debug.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdio.h>

using namespace Eigen;
using namespace MPCWalkgen;

void makeScilabFile(std::string type, double time);
void dumpTrajectory(MPCSolution & result, std::vector<std::ofstream*> & data_vec);
bool checkFiles(std::ifstream & f1, std::ifstream & f2);
int copyFile(const std::string & source, const std::string & destination);

int main() {

	// Logging:
	// --------
	const int nbFile=6;

	std::vector<std::string> name_vec(nbFile);
	name_vec[0]="CoM_X";
	name_vec[1]="CoM_Y";
	name_vec[2]="CoP_X";
	name_vec[3]="CoP_Y";
	name_vec[4]="BAS_X";
	name_vec[5]="BAS_Y";

	std::vector<std::ofstream*> data_vec(nbFile);
	std::vector<std::ifstream*> ref_vec(nbFile);
	for (int i = 0; i < nbFile; ++i) {
		data_vec[i] = new std::ofstream((name_vec[i]+".data").c_str());
		ref_vec[i] = new std::ifstream((name_vec[i]+".ref").c_str());
	}


	MPCData mpcData;
	RobotData robotData;
	MPCDebug debug(true);

	// Creat and initialize generator:
	// -------------------------------
	Walkgen walk(LSSOL);
	walk.init(robotData, mpcData);

	// Run:
	// ----
	double velocity = 0.25;
	walk.reference(0, 0, 0);
	walk.online(0);
	double t = 0;

	BodyState CoMState=walk.state(COM);
	CoMState.x[0]=-0.02;
	CoMState.x[1]=0;
	CoMState.x[2]=0;
	CoMState.y[0]=-0.01;
	CoMState.y[1]=0;
	CoMState.y[2]=0;
	BodyState baseState=walk.state(BASE);
	baseState.x[0]=0.02;
	baseState.x[1]=0;
	baseState.x[2]=0;
	baseState.y[0]=0.01;
	baseState.y[1]=0;
	baseState.y[2]=0;
	walk.state(COM,CoMState);
	walk.state(BASE,baseState);

	for (; t < 10; t += 0.02){
		walk.reference(0, 0, 0);
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}

	Eigen::VectorXd zero(10);
	Eigen::VectorXd vel(10);
	zero.fill(0);
	for (; t < 20; t += 0.02){
		for(int i=0;i<10;++i){
		  vel(i)=velocity*(t+i*0.02-10)/10;
		}
		walk.reference(vel, zero, zero);
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}
	CoMState=walk.state(COM);
	CoMState.x[1]=0;
	CoMState.x[2]=0;
	CoMState.y[1]=0;
	CoMState.y[2]=0;
	baseState=walk.state(BASE);
	baseState.x[1]=0;
	baseState.x[2]=0;
	baseState.y[1]=0;
	baseState.y[2]=0;
	for (; t < 30 ; t += 0.02){
		walk.state(COM,CoMState);
		walk.state(BASE,baseState);
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}
	for (; t < 40; t += 0.02){
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}
	walk.reference(0, velocity, 0);
	for (; t < 50; t += 0.02){
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}
	walk.ApplyPerturbationForce(X, COM, 100);
	for (; t < 60; t += 0.02){
		debug.getTime(1,true);
		MPCSolution result = walk.online(t);
		debug.getTime(1,false);
		dumpTrajectory(result, data_vec);
	}

	// Reopen the files:
	// -----------------
	std::vector<std::ifstream*> check_vec(nbFile);
	for(int i = 0;i < nbFile; ++i){
		check_vec[i] = new std::ifstream((name_vec[i]+".data").c_str());
	}


	bool success = true;
	for(unsigned i = 0; i < check_vec.size();++i){
		// if the reference file exists, compare with the previous version.
		if (*ref_vec[i]){
			if (!checkFiles(*check_vec[i],*ref_vec[i])){
				success = false;
			}
		}
		// otherwise, create it
		else{
			copyFile((name_vec[i]+".data"), (name_vec[i]+".ref"));
		}
		check_vec[i]->close();
		ref_vec[i]->close();
	}
	makeScilabFile("data", t);
	makeScilabFile("ref", t);

	for (int i=0; i < nbFile; ++i) {
		delete check_vec[i];
		delete ref_vec[i];
		delete data_vec[i];
	}
	std::cout << "Mean iteration duration with LSSOL   : " << debug.computeInterval(1) << " us" << std::endl;
	return (success)?0:1;
}

void dumpTrajectory(MPCSolution &result, std::vector<std::ofstream*> &data_vec) {
	for (int i = 0; i < result.state_vec[0].CoMTrajX_.rows(); ++i) {
		*data_vec[0] << result.state_vec[0].CoMTrajX_(i) << " " << result.state_vec[1].CoMTrajX_(i) << " " << result.state_vec[2].CoMTrajX_(i) << std::endl;
		*data_vec[1] << result.state_vec[0].CoMTrajY_(i) << " " << result.state_vec[1].CoMTrajY_(i) << " " << result.state_vec[2].CoMTrajY_(i) << std::endl;
		*data_vec[2] << result.CoPTrajX(i)  << std::endl;
		*data_vec[3] << result.CoPTrajY(i) << std::endl;
		*data_vec[4] << result.state_vec[0].baseTrajX_(i) << " " << result.state_vec[1].baseTrajX_(i) << " " << result.state_vec[2].baseTrajX_(i) << std::endl;
		*data_vec[5] << result.state_vec[0].baseTrajY_(i) << " " << result.state_vec[1].baseTrajY_(i) << " " << result.state_vec[2].baseTrajY_(i) << std::endl;
	}
}

void makeScilabFile(std::string type, double time) {
	std::ofstream sci(("plot"+type+".sci").c_str());
	sci << "stacksize(268435454);" << std::endl;
	sci << "X=read('CoM_X." << type << "',-1,3);" << std::endl;
	sci << "Y=read('CoM_Y." << type << "',-1,3);" << std::endl;
	sci << "ZX=read('CoP_X." << type << "',-1,1);" << std::endl;
	sci << "ZY=read('CoP_Y." << type << "',-1,1);" << std::endl;
	sci << "BX=read('BAS_X." << type << "',-1,3);" << std::endl;
	sci << "BY=read('BAS_Y." << type << "',-1,3);" << std::endl;
	sci << "s=size(X);t=linspace(0,"<<time<<",s(1));" << std::endl;
	sci << "subplot(2,1,1);" << std::endl;
	sci << "plot(t,[X(:,1),ZX(:,1),BX(:,1)]);" << std::endl;
	sci << "title('');" << std::endl;
	sci << "subplot(2,1,2);" << std::endl;
	sci << "plot(t,[Y(:,1),ZY(:,1),BY(:,1)]);" << std::endl;
	sci << "title('');" << std::endl;

	sci << "scf();" << std::endl;

	sci << "subplot(2,2,1);" << std::endl;
	sci << "plot(t,X);" << std::endl;
	sci << "title('CoM_X');" << std::endl;
	sci << "subplot(2,2,3);" << std::endl;
	sci << "plot(t,Y);" << std::endl;
	sci << "title('CoM_Y');" << std::endl;

	sci << "subplot(2,2,2);" << std::endl;
	sci << "plot(t,ZX);" << std::endl;
	sci << "title('CoP_X');" << std::endl;
	sci << "subplot(2,2,4);" << std::endl;
	sci << "plot(t,ZY);" << std::endl;
	sci << "title('CoP_Y');" << std::endl;

	sci << "scf();" << std::endl;

	sci << "plot(X(:,1),Y(:,1),ZX,ZY);" << std::endl;
	sci << "title('CoM_and_CoP_traj');" << std::endl;

	sci << "scf();" << std::endl;

	sci << "subplot(2,1,1);" << std::endl;
	sci << "plot(t,BX);" << std::endl;
	sci << "title('base_X');" << std::endl;
	sci << "subplot(2,1,2);" << std::endl;
	sci << "plot(t,BY);" << std::endl;
	sci << "title('base_Y');" << std::endl;


	sci << "scf();" << std::endl;

	sci << "plot(BX(:,1),BY(:,1));" << std::endl;
	sci << "title('Base_traj');" << std::endl;

	sci.close();
}

bool checkFiles(std::ifstream & fich1, std::ifstream & fich2) {
	bool equal=1;
	if (fich1 && fich2) {
		std::string lignef1;
		std::string lignef2;
		while (std::getline( fich1, lignef1) && std::getline( fich2, lignef2) && equal) {
			if (strcmp(lignef1.c_str(),lignef2.c_str())!=0) {
				equal = 0;
			}
		}
	}

	return equal;
}

int copyFile(const std::string & source, const std::string & destination) {
	std::ifstream ifs(source.c_str(), std::ios::binary);
	std::ofstream ofs(destination.c_str(), std::ios::binary);
	ofs << ifs.rdbuf();
	ofs.close();
	ifs.close();

	return 0;
}
