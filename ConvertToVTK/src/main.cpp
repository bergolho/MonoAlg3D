//============================================================================
// Name        : main.cpp
// Author      : Rafael Sachetto
// Version     :
// Copyright   : Your copyright notice
// Description : Convert Monodomain text output to VTK
//============================================================================

#include <iostream>
#include "criaVTK.h"
#include "file_util.h"
#include "opts.h"

int main(int argc, char**argv) {

    parseOptions(argc, argv);

    std::vector<std::string> solutions;
    std::string dir(globalArgs.inDirName);
    getFilesFromDir(dir, solutions, "V_t_");
    convertAlgToVTK3D(solutions, globalArgs.binary, globalArgs.adaptive);
    //convertAlgToVTK3D(solutions, 52000.0, globalArgs.binary);

}

