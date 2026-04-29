/* Copyright 2023, Gurobi Optimization, LLC */

/* This example formulates and solves the following simple MIP model:

     maximize    x +   y + 2 z
     subject to  x + 2 y + 3 z <= 4
                 x +   y       >= 1
                 x, y, z binary
*/

#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <string.h>
#include <algorithm>
#include <array>
#include <functional>
#include <string_view>
using namespace std;

int
main(int   argc,
    char* argv[])
{
    try {

        // Create an environment
        GRBEnv env = GRBEnv(true);
        env.set("LogFile", "mip1.log");
        env.start();



        // ***************************************************** Sets ********************************************
        int nCrops = 25;
        int nFarmers = 1;
        int nPeriods = 36;
        int nWateringTech = 6;
        int nPrecScenarios = 3;
        int nPriceScenarios = 3;
        int nYieldScenarios = 3;
        int nScenarios = 27;
        int nPareto = 10;
        string CropList[] =
        { "arpa", "aspir", "aycicegi", "balkabagi", "biber", "bugday", "ceviz", "domates", "elma", "erik", "fasulye", "hiyar", "karpuz", "kavun", "kayisi", "kiraz", "maydonoz", "mercimek", "misir", "nohut" , "patates", "sogan", "yonca", "sekerpancari", "uzum" };
        string DripWateringTechList[] =
        { "mobiledrip", "stabledrip", "linearmove","centralpivot", "micro", "surfacedrip" };
        string WateringTechList[] =
        { "drip", "surface" };
        // ***************************************************** Parameters ********************************************
                // Input file

        ifstream myReadFile;
        myReadFile.open("ETc.txt");
        std::vector<std::vector<double>> ETc;
        while (!myReadFile.eof()) {
            for (int i = 0; i < nCrops; i++) {
                vector<double> tmpVec;
                double tmpString;

                for (int j = 0; j < nPeriods; j++) {
                    myReadFile >> tmpString;
                    tmpVec.push_back(tmpString);
                }
                ETc.push_back(tmpVec);
            }
        }

        ifstream myPriceFile;
        myPriceFile.open("price.txt");
        std::vector<double> Price;
        while (!myPriceFile.eof()) {
            for (int i = 0; i < nCrops; i++) {
                double tmpString;
                myPriceFile >> tmpString;
                Price.push_back(tmpString);
            }
        }
        ifstream myYieldFile;
        myYieldFile.open("yield.txt");
        std::vector<double> Yield;
        while (!myYieldFile.eof()) {
            for (int i = 0; i < nCrops; i++) {
                double tmpString;
                myYieldFile >> tmpString;
                Yield.push_back(tmpString);
            }
        }
        ifstream myPrecipFile;
        myPrecipFile.open("precipitation.txt");
        std::vector<double> Precipitation;
        while (!myPrecipFile.eof()) {
            for (int i = 0; i < nPeriods; i++) {
                double tmpString;
                myPrecipFile >> tmpString;
                Precipitation.push_back(tmpString);
            }
        }
        /*ifstream myWaterTargetFile;
        myWaterTargetFile.open("watercap.txt");
        std::vector<double> TargetWatCap;
        while (!myWaterTargetFile.eof()) {
            for (int i = 0; i < nPeriods; i++) {
                double tmpString;
                myWaterTargetFile >> tmpString;
                TargetWatCap.push_back(tmpString);
            }
        }*/
        std::vector < std::vector<string>> Croptype;
        std::vector<string> row;
        string line, word;
        ifstream file("croptype.csv", ios::in);
        if (file.is_open())
        {
            while (getline(file, line))
            {
                row.clear();
                stringstream str(line);
                while (getline(str, word, ','))
                    row.push_back(word);
                Croptype.push_back(row);
            }

        }

        // Target Water capacity for period in tons
        double TargetWatCap = 7980000000;   //Total for region per month in kg
        double TargetRevenue = 3488308000 * 0.33;  //TL (2021 values)
        double TotAreaCap = 5553358000 * 0.33; //m2
        //double AppliedWaterEfficiency[] = { 0.7, 0.8, 0.9 };
        double AppliedWaterEfficiency = 0.8;
        double WaterCarryingEfficiency = 0.98;
        double ShadedRegion[] = { 0.8, 0.75, 0.7 };
        double budgetdeficitUB = TargetRevenue * 0.7;
        //double extraneedUB = 100; //in tons

        std::vector<std::vector<std::vector<double>>> totwaterneed;
        for (int c = 0; c < nCrops; ++c)
        {
            std::vector<std::vector<double>> tempBiVec;
            for (int p = 0; p < nPeriods; ++p)
            {
                std::vector<double> tempInVec;
                for (int s = 0; s < nScenarios; ++s)
                {
                    double calculated_surf = 0;
                    double calculated_rain = 0;
                    double calculated_drip = 0;
                    double weight_surf = 0;
                    double weight_rain = 0;
                    double weight_drip = 0;
                    double calculated = 0;
                    const char* cstr = Croptype[c][1].c_str();
                    double shadedRegPercToUse = 0;
                    if (strcmp(cstr, "1") == 0)
                    {
                        shadedRegPercToUse = ShadedRegion[0];
                        weight_surf = 1.0;
                        weight_rain = 0.0;
                        weight_drip = 0.0;
                    }
                    else if (strcmp(cstr, "2") == 0) {
                        shadedRegPercToUse = ShadedRegion[1];
                        weight_surf = 0.6;
                        weight_rain = 0.23;
                        weight_drip = 0.17;
                    }
                    else {
                        shadedRegPercToUse = ShadedRegion[2];
                        weight_surf = 0.6;
                        weight_rain = 0.23;
                        weight_drip = 0.17;
                    }
                    calculated_rain = ETc[c][p] * (shadedRegPercToUse / 0.85);
                    calculated_drip = ETc[c][p] * (shadedRegPercToUse / 0.85);
                    //Based on precipitation 3 scenarios will be added
                    double Pe = Precipitation[p];
                    if (s < 9)
                    {
                        calculated_surf = std::fmax(0, ETc[c][p] - (Pe * 0.8 * 0.9));
                        calculated_rain = std::fmax(0, calculated_rain - (Pe * 0.8 * 0.9));
                        calculated_drip = std::fmax(0, calculated_drip - (Pe * 0.8 * 0.9));
                    }
                    else if (s >= 9 && s < 18) {
                        calculated_surf = std::fmax(0, ETc[c][p] - (Pe * 0.8));
                        calculated_rain = std::fmax(0, calculated_rain - (Pe * 0.8));
                        calculated_drip = std::fmax(0, calculated_drip - (Pe * 0.8));
                    }
                    else {
                        calculated_surf = std::fmax(0, ETc[c][p] - (Pe * 0.8 * 1.1));
                        calculated_rain = std::fmax(0, calculated_rain - (Pe * 0.8 * 1.1));
                        calculated_drip = std::fmax(0, calculated_drip - (Pe * 0.8 * 1.1));
                    }
                    // water conducting efficiency is assumed 98% for all methods
                    calculated_rain = calculated_rain / (0.98 * 0.75); //watering efficiency for steady raining method 75%
                    calculated_drip = calculated_drip / (0.98 * 0.95); //watering efficiency for beneath surface dripping method 95%
                    calculated = calculated_surf * weight_surf + calculated_rain * weight_rain + calculated_drip * weight_drip;
                    // usage percentages of each method are from 2021 dsi data

                    tempInVec.push_back(calculated);
                }
                tempBiVec.push_back(tempInVec);
            }
            totwaterneed.push_back(tempBiVec);
        }
        std::vector<std::vector<double>> yieldsce;
        for (int c = 0; c < nCrops; ++c)
        {
            double calculated = 0;
            std::vector<double> tempInVec;
            for (int s = 0; s < nScenarios; ++s)
            {
                if ((s % 9) < 3)
                {
                    tempInVec.push_back(Yield[c] * 0.75);
                }
                else if ((s % 9) >= 3 && (s % 9) < 6) {
                    tempInVec.push_back(Yield[c] * 1.0);
                }
                else { tempInVec.push_back(Yield[c] * 1.25); }
            }
            yieldsce.push_back(tempInVec);
        }
        std::vector<std::vector<double>> pricesce;
        for (int c = 0; c < nCrops; ++c)
        {
            double calculated = 0;
            std::vector<double> tempInVec;
            for (int s = 0; s < nScenarios; ++s)
            {
                if (s % 3 == 0)
                {
                    tempInVec.push_back(Price[c] * 0.5);
                }
                else if (s % 3 == 1) {
                    tempInVec.push_back(Price[c] * 1.0);
                }
                else { tempInVec.push_back(Price[c] * 1.5); }
            }
            pricesce.push_back(tempInVec);
        }


        for (int wcount = 0; wcount < (nPareto + 1); ++wcount) {
            double weightrevenue = wcount * (1.0 / nPareto);
            double weightwater = 1.0 - weightrevenue;
            //weightrevenue = 0.5;
            //weightwater = 0.5;
            //*********************************************************** Model components**************************************
            // Create an empty model
            GRBModel model = GRBModel(env);


            // revenue decision variables: how much revenue is earned from crop c for scenario s
            GRBVar** revenue = 0;
            revenue = new GRBVar * [nCrops];
            for (int c = 0; c < nCrops; ++c)
            {
                revenue[c] = model.addVars(nScenarios);
                for (int s = 0; s < nScenarios; ++s)
                {
                    ostringstream vname;
                    vname << "revenue" << "_" << CropList[c] << "_S" << s;
                    revenue[c][s].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                    revenue[c][s].set(GRB_StringAttr_VarName, vname.str());
                    revenue[c][s].set(GRB_DoubleAttr_Obj, (-1.0 * weightrevenue) / (9600000000));
                }
            }
            // savedwater decision variables: how much water is saved from the target for scenario s during period p
            GRBVar** savedwater = 0;
            savedwater = new GRBVar * [nPeriods];
            for (int p = 0; p < nPeriods; ++p)
            {
                savedwater[p] = model.addVars(nScenarios);
                for (int s = 0; s < nScenarios; ++s)
                {
                    ostringstream vname;
                    vname << "savedwater" << "_P" << p << "_S" << s;
                    savedwater[p][s].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                    savedwater[p][s].set(GRB_DoubleAttr_LB, 0);
                    savedwater[p][s].set(GRB_StringAttr_VarName, vname.str());
                }
            }// water related decision variables: how much extra water is needed from the target for scenario s during period p
            GRBVar** extraneed = 0;
            extraneed = new GRBVar * [nPeriods];
            for (int p = 0; p < nPeriods; ++p)
            {
                extraneed[p] = model.addVars(nScenarios);
                for (int s = 0; s < nScenarios; ++s)
                {
                    ostringstream vname;
                    vname << "extraneed" << "_P" << p << "_S" << s;

                    extraneed[p][s].set(GRB_DoubleAttr_UB, (0.1 * TargetWatCap));
                    extraneed[p][s].set(GRB_DoubleAttr_LB, 0);
                    extraneed[p][s].set(GRB_StringAttr_VarName, vname.str());
                    //extraneed[p][s].set(GRB_DoubleAttr_Obj, (weightwater / (1.5 * TargetWatCap)));
                }
            }
            // extragain decision variables: how much extra money is made from the target for scenario s during period p
            GRBVar* extragain = 0;
            extragain = model.addVars(nScenarios);
            for (int s = 0; s < nScenarios; ++s)
            {
                ostringstream vname;
                vname << "extragain" << "_S" << s;
                extragain[s].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                extragain[s].set(GRB_DoubleAttr_LB, 0);
                extragain[s].set(GRB_StringAttr_VarName, vname.str());
                extragain[s].set(GRB_DoubleAttr_UB, (TargetRevenue));
                //extragain[s].set(GRB_DoubleAttr_Obj, (weightrevenue / (-0.1 * TargetRevenue)));
            }

            // budgetdeficit decision variables: how much is below from the target budget for scenario s during period p
            GRBVar* budgetdeficit = 0;

            budgetdeficit = model.addVars(nScenarios);
            for (int s = 0; s < nScenarios; ++s)
            {
                ostringstream vname;
                vname << "budgetdeficit" << "_S" << s;

                budgetdeficit[s].set(GRB_DoubleAttr_UB, budgetdeficitUB);
                budgetdeficit[s].set(GRB_DoubleAttr_LB, 0);
                budgetdeficit[s].set(GRB_StringAttr_VarName, vname.str());
            }

            GRBVar** area = 0;
            area = new GRBVar * [nFarmers];
            for (int f = 0; f < nFarmers; f++) {
                area[f] = model.addVars(nCrops);;
                for (int c = 0; c < nCrops; c++) {

                    ostringstream vname;
                    vname << "area_F" << f << "_" << CropList[c]; // in m2
                    area[f][c].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                    area[f][c].set(GRB_StringAttr_VarName, vname.str());

                }
            }
            GRBVar*** totwatercons = 0;
            totwatercons = new GRBVar * *[nPeriods];
            for (int p = 0; p < nPeriods; ++p) {
                totwatercons[p] = new GRBVar * [nCrops];
                for (int c = 0; c < nCrops; c++) {
                    totwatercons[p][c] = model.addVars(nScenarios);
                    for (int s = 0; s < nScenarios; s++) {
                        ostringstream vname;
                        vname << "totwatercons_P" << p << "_" << CropList[c] << "_S" << s; // in kgs
                        totwatercons[p][c][s].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                        totwatercons[p][c][s].set(GRB_StringAttr_VarName, vname.str());
                    }
                }
            }
            GRBVar* grndwatercons = 0;
            grndwatercons = model.addVars(nScenarios);
            for (int s = 0; s < nScenarios; s++) {
                ostringstream vname;
                vname << "grndwatercons" << "_S" << s; // in kgs
                grndwatercons[s].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
                grndwatercons[s].set(GRB_StringAttr_VarName, vname.str());
                grndwatercons[s].set(GRB_DoubleAttr_Obj, (weightwater) / (TargetWatCap));

            }
            //GRBVar maxBudgetDeficit = model.addVar(0.0, budgetdeficitUB, (weightrevenue / budgetdeficitUB), GRB_CONTINUOUS, "maxBudgetDeficit");;


            //GRBVar maxExtraNeed = model.addVar(0.0, TargetWatCap, (weightwater / TargetWatCap), GRB_CONTINUOUS, "maxExtraNeed_P");


            //***********************************************************************************CONSTRAINTS*************************************************************
            for (int p = 0; p < nPeriods; ++p)
            {
                for (int c = 0; c < nCrops; c++)
                {
                    for (int s = 0; s < nScenarios; s++)
                    {
                        GRBLinExpr ptot = 0;
                        for (int f = 0; f < nFarmers; ++f)
                        {
                            ptot += totwaterneed[c][p][s] * area[f][c];
                        }
                        ostringstream cname;
                        cname << "WaterUse.P" << p << "." << CropList[c] << ".S" << s;
                        model.addConstr(ptot == totwatercons[p][c][s], cname.str());
                    }
                }
            }
            for (int s = 0; s < nScenarios; s++)
            {
                GRBLinExpr ptot = 0;
                for (int f = 0; f < nFarmers; ++f)
                {
                    for (int c = 0; c < nCrops; c++)
                    {
                        ptot += area[f][c];

                    }
                }
                ostringstream cname;
                cname << "AreaCap.S" << s;
                model.addConstr(ptot <= TotAreaCap, cname.str());
            }

            for (int p = 0; p < nPeriods; ++p)
            {
                for (int c = 0; c < nCrops; c++)
                {
                    for (int s = 0; s < nScenarios; s++)
                    {
                        GRBLinExpr ptot = 0;
                        for (int f = 0; f < nFarmers; ++f)
                        {
                            ptot += yieldsce[c][s] * pricesce[c][s] * area[f][c];
                        }
                        ostringstream cname;
                        cname << "Revenuedef.P" << p << "." << CropList[c] << ".S" << s;
                        model.addConstr(ptot == revenue[c][s], cname.str());
                    }
                }
            }

            for (int s = 0; s < nScenarios; s++)
            {
                GRBLinExpr ptot = 0;
                for (int p = 0; p < nPeriods; ++p)
                {
                    for (int c = 0; c < nCrops; c++)
                    {
                        ptot += totwatercons[p][c][s];
                    }
                    //ptot += savedwater[p][s] - extraneed[p][s];
                }
                ostringstream cname;
                cname << "DeviateWatTar" << ".S" << s;

                model.addConstr(ptot <= TargetWatCap, cname.str()); //TargetWatCap is in kgs
            }

            for (int s = 0; s < nScenarios; s++)
            {
                GRBLinExpr ptot = 0;
                for (int p = 0; p < nPeriods; ++p)
                {
                    for (int c = 0; c < nCrops; c++)
                    {
                        ptot += totwatercons[p][c][s];
                    }
                }
                ostringstream cname;
                cname << "GrndWaterDef" << ".S" << s;
                model.addConstr(ptot == grndwatercons[s], cname.str());
            }


            for (int s = 0; s < nScenarios; s++)
            {
                GRBLinExpr ptot = 0;
                for (int c = 0; c < nCrops; c++)
                {
                    ptot += revenue[c][s];
                }
                ptot += budgetdeficit[s] - extragain[s];
                ostringstream cname;
                cname << "DeviateRevTar" << ".S" << s;
                model.addConstr(ptot == TargetRevenue, cname.str());
            }


            /*
            for (int s = 0; s < nScenarios; s++)
            {
                GRBLinExpr ptot = 0;
                for (int p = 0; p < nPeriods; ++p)
                {
                    ptot += extraneed[p][s];
                }
                ostringstream cname;
                cname << "MaxExtraNeedDef" << ".S" << s;
                model.addConstr(ptot <= maxExtraNeed, cname.str());
            }



            for (int s = 0; s < nScenarios; s++)
            {
                ostringstream cname;
                cname << "MaxBudgetDeficitDef.S" << s;
                model.addConstr(budgetdeficit[s] <= maxBudgetDeficit, cname.str());
            }*/
            //***********************************************************************************OPTIMIZE*************************************************************

            model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);


            // Optimize model
            model.optimize();

            model.write("ParetoReg.lp");
            //model.write("watermodel.mps");
    //**************************************************************************OUTPUTS**************************************************************************
            std::ofstream myfile;
            ostringstream paretofilename;
            paretofilename << "ParetoReg_P" << wcount << ".csv";
            myfile.open(paretofilename.str());
            myfile << "MinRevenue,MinRevenueSce,MaxRevenue,MaxRevenueSce,MedRevenue,\n";
            double minvalue = 100000000000;
            double medianvalue = 0;
            double maxvalue = 0;
            int minscenario = 0;
            int medianscenario = 0;
            int maxscenario = 0;
            std::array<double, 27> ResRevenue;
            std::array<double, 27> ResWaterCons;
            for (int s = 0; s < nScenarios; s++) {
                double revenuetotal = 0;
                for (int c = 0; c < nCrops; c++) {
                    int countpositivesce = 0;

                    if (revenue[c][s].get(GRB_DoubleAttr_X) > 1) {
                        revenuetotal += revenue[c][s].get(GRB_DoubleAttr_X);
                    }

                }
                ResRevenue[s] = revenuetotal;
                if (revenuetotal >= maxvalue) {
                    maxvalue = revenuetotal;
                    maxscenario = s;
                }
                if (revenuetotal < minvalue) {
                    minvalue = revenuetotal;
                    minscenario = s;
                }

            }
            //int compare(const void* a, const void* b) { return (*(int*)a - *(int*)b);}

            std::sort(ResRevenue.begin(), ResRevenue.end());
            myfile << minvalue << "," << minscenario << "," << maxvalue << "," << maxscenario << "," << ResRevenue[13] << "\n";
            minvalue = 100000000000;
            medianvalue = 0;
            maxvalue = 0;
            minscenario = 0;
            medianscenario = 0;
            maxscenario = 0;
            for (int s = 0; s < nScenarios; s++) {
                double watertotal = 0;
                for (int c = 0; c < nCrops; c++) {

                    for (int p = 0; p < nPeriods; ++p) {
                        if (totwatercons[p][c][s].get(GRB_DoubleAttr_X) > 1) {
                            watertotal += totwatercons[p][c][s].get(GRB_DoubleAttr_X);
                        }
                    }

                }
                ResWaterCons[s] = watertotal;

                if (watertotal >= maxvalue) {
                    maxvalue = watertotal;
                    maxscenario = s;
                }
                if (watertotal < minvalue) {
                    minvalue = watertotal;
                    minscenario = s;
                }

            }
            std::sort(ResWaterCons.begin(), ResWaterCons.end());
            myfile << minvalue << "," << minscenario << "," << maxvalue << "," << maxscenario << "," << ResWaterCons[13] << "\n";
            myfile << "Area Variable,m2,\n";
            for (int f = 0; f < nFarmers; f++) {
                for (int c = 0; c < nCrops; c++) {
                    if (area[f][c].get(GRB_DoubleAttr_X) > 1) {
                        myfile << CropList[c].c_str() << "," << area[f][c].get(GRB_DoubleAttr_X) << "\n";
                    }
                }
            }
            myfile << "Obj: " << model.get(GRB_DoubleAttr_ObjVal) << endl;

            myfile << "\n" << "\n" << "\n" << "\n";
            /*
            for (int s = 0; s < nScenarios; s++) {
                int countpositivesce = 0;
                double watertotal = 0;
                for (int c = 0; c < nCrops; c++)
                {

                    for (int p = 0; p < nPeriods; ++p) {
                        if (totwatercons[p][c][s].get(GRB_DoubleAttr_X) > 1) {
                            watertotal += totwatercons[p][c][s].get(GRB_DoubleAttr_X);
                            if (countpositivesce == 0) { countpositivesce += 1; }
                        }
                    }
                }
                if (watertotal > 1) {
                    myfile << s << "," << (watertotal) << "\n";
                }
            }

            myfile << "Crop Area Stats,m2,\n";
            for (int f = 0; f < nFarmers; f++) {
                for (int c = 0; c < nCrops; c++) {
                    int countpositivesce = 0;
                    double areatotal = 0;
                    for (int s = 0; s < nScenarios; s++) {
                        if (area[f][c][s].get(GRB_DoubleAttr_X) > 1) {
                            areatotal = areatotal + area[f][c][s].get(GRB_DoubleAttr_X);
                            countpositivesce += 1;
                        }
                    }
                    if (areatotal > 1) {
                        myfile << CropList[c].c_str() << "," << (areatotal / countpositivesce) << "\n";
                    }
                }
            }*/


            myfile << "WaterConsumed,kg,\n";
            for (int p = 0; p < nPeriods; ++p)
            {
                for (int c = 0; c < nCrops; c++) {
                    for (int s = 0; s < nScenarios; s++) {
                        if (totwatercons[p][c][s].get(GRB_DoubleAttr_X) > 1) {
                            myfile << totwatercons[p][c][s].get(GRB_StringAttr_VarName) << "," << totwatercons[p][c][s].get(GRB_DoubleAttr_X) << "\n";
                        }
                    }
                }
            }
            myfile << "\n" << "\n" << "\n" << "\n";
            myfile << "Average Revenue,TL(April 2021),TL(April 2023),\n";
            myfile << "Revenue,TL(April 2023),\n";
            for (int c = 0; c < nCrops; c++) {
                for (int s = 0; s < nScenarios; s++) {
                    if (revenue[c][s].get(GRB_DoubleAttr_X) > 1) {
                        myfile << revenue[c][s].get(GRB_StringAttr_VarName) << "," << revenue[c][s].get(GRB_DoubleAttr_X) << "\n";
                    }
                }
            }
            myfile << "\n" << "\n" << "\n" << "\n";
            myfile << "p,s,savedwater,extraneed,budgetdeficit,extragain,\n";
            for (int p = 0; p < nPeriods; ++p)
            {
                for (int s = 0; s < nScenarios; s++)
                {
                    myfile << p << "," << s << "," << savedwater[p][s].get(GRB_DoubleAttr_X) << "," << extraneed[p][s].get(GRB_DoubleAttr_X) << "," << budgetdeficit[s].get(GRB_DoubleAttr_X) << "," << extragain[s].get(GRB_DoubleAttr_X) << "\n";
                }

            }
            cout << "Pareto: " << wcount << " Obj: " << model.get(GRB_DoubleAttr_ObjVal) << endl;




            //********************* Delete variables
            for (int f = 0; f < nFarmers; f++) {
                delete[] area[f];
            }
            delete[] area;

            for (int i = 0; i < nCrops; i++) {
                delete[] revenue[i];
            }
            delete[] revenue;

            for (int i = 0; i < nPeriods; i++) {
                delete[] savedwater[i];
            }
            delete[] savedwater;

            for (int i = 0; i < nPeriods; i++) {
                delete[] extraneed[i];
            }
            delete[] extraneed;

            delete[] extragain;
            delete[] budgetdeficit;
            delete[] grndwatercons;
            //delete[] maxExtraNeed;
            for (int i = 0; i < nPeriods; i++) {
                for (int j = 0; j < nCrops; j++) {
                    delete[] totwatercons[i][j];
                }
                delete[] totwatercons[i];
            }
            delete[] totwatercons;
        }// end of weight loop
    }
    catch (GRBException e) {
        cout << "Error code = " << e.getErrorCode() << endl;
        cout << e.getMessage() << endl;
    }
    catch (...) {
        cout << "Exception during optimization" << endl;
    }

    return 0;
}
