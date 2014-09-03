#include "PTGSolver.hpp"

#include "stdlib.h"
#include <iostream>
#include <fstream>
#include <list>

using namespace std;

PTGSolver::PTGSolver(){
	size = 0;
	ptg = NULL;
	size = 0;
	time = 0;
}

void PTGSolver::solvePTG(PTG* p, bool visu){
	cout << "====SolvePTG====" << endl;
	ptg = p;
	size = ptg->getSize();
	if(size > 0){
		int copyNb = ptg->getNbResets();
		createResets();
		while (copyNb >= 0){
			cout << "====Solving copy nb: " << copyNb << " ====" << endl;

			createEndPoints();

			Value endM = endPoints.back();
			endPoints.pop_back();
			time = endM;
			if(vals.size() == 0)
				init();//Init is after createEndPoints because the time needs to be updated before creating the Strategy object

			//We need to reset everything
			strategies.push_front(Strategy(size, time, true));
			strategies.front().insert(0,0,false);
			for (unsigned int i = 1; i < size; ++i){
				vals[i].setInf(true);

				pathsLengths.push_back(Value());
				//To force taking the first transitions available.
				if(ptg->getOwner(i))
					pathsLengths[i].setInf(true);
				strategies.front().insert(i, -1, false);
			}
			//Update the transitions that can be taken between the two given parameters
			keepTransAvailable(time, time);


			//First extendedDijkstra on the biggest "M"
			cout << "====First extended Dijkstra====" << endl;
			PGSolver* pgSolver = new PGSolver(ptg, &pathsLengths, &vals, &strategies, &resets);
			pgSolver->extendedDijkstra(false);
			delete pgSolver;
			//Updating the value functions
			for (unsigned int i = 0; i < size; ++i){
				valueFcts[i].push_front(Point(time,vals[i]));
			}
			//show();

			while(!endPoints.empty()){


				time = endPoints.back();
				cout << "===TIME: " << time << " ===" << endl;

				endPoints.pop_back();
				//First ExtendedDijkstra done at the last "M"
				strategies.push_front(Strategy(size, endM, false));
				keepTransAvailable(time, endM);
				updateBottoms();

				pgSolver = new PGSolver(ptg, &pathsLengths, &vals, &strategies, &bottoms, &resets);

				pgSolver->extendedDijkstra(true);
				delete pgSolver;

				//show();

				SPTGSolver* sptgSolver = new SPTGSolver(ptg, &bottoms, &pathsLengths, &vals, &strategies, &valueFcts, &resets);
				sptgSolver->solveSPTG();
				delete sptgSolver;

				//The resolution of a SPTG is done between 0 and 1, we need to rescale the valueFcts
				rescale(time, endM);
				//show();

				//The last step is to do an extendedDijkstra on the game in the "time" instant
				keepTransAvailable(time, time);
				updateBottoms();
				strategies.push_front(Strategy(size, time, true));




				pgSolver = new PGSolver(ptg, &pathsLengths, &vals, &strategies, &bottoms, &resets);
				pgSolver->extendedDijkstra(true);
				for (unsigned int i = 0; i < size; ++i){
					valueFcts[i].push_front(Point(time,vals[i]));
				}

				delete pgSolver;
				endM = time;
				//show();
				//cleanValueFcts();
				//cleanStrats();
			}

			restoreAllTrans();
			updateResets();
			//show();
			--copyNb;

		}
		correctStrats();

		//show();
		if(visu){
			cerr << "Drawing..." << endl;
			visualize();
		}
		cleanValueFcts();
		cout << endl << "====Results SolvePTG===" << endl;
		show();
	}
	else
		cout << "The game is empty" << endl;

}

void PTGSolver::init(){
	vals.push_back(CompositeValue());
	pathsLengths.push_back(0);
	bottoms.push_back(0);
	valueFcts.push_back(list<Point>());

	for (unsigned int i = 1; i < size; ++i){
		vals.push_back(CompositeValue());
		bottoms.push_back(0);
		valueFcts.push_back(list<Point>());
	}
}

void PTGSolver::createEndPoints(){
	//   Create all endpoints where some calculation will be needed
	for (unsigned int i = 0; i < size; ++i){
		for (unsigned int j = 0; j < size; ++j){
			if(ptg->getTransition(i,j) != -1){
				endPoints.push_back(ptg->getStartCst(i,j));
				endPoints.push_back(ptg->getEndCst(i,j));
			}
		}
	}

	endPoints.sort();
	endPoints.unique();

}

void PTGSolver::keepTransAvailable(Value start, Value end){
	cout << "===Updating Transitions (" << start << "," << end << ") ====" << endl;

	//Restore the transitions stored
	list<Transition>::iterator next = storage.begin();
	for (list<Transition>::iterator it = storage.begin(); it != storage.end(); it = next){
		++next;
		if(Value(ptg->getStartCst(it->origin, it->dest)) <= start && Value(ptg->getEndCst(it->origin, it->dest)) >= end){
			ptg->setTransition(it->origin, it->dest, it->cost);

			storage.erase(it);

		}

	}

	//Store the ones that can't be taken
	for (unsigned int i = 0; i < size; ++i){
		for (unsigned int j = 0; j < size; ++j){
			if(ptg->getTransition(i,j) != -1 && (Value(ptg->getStartCst(i,j)) > start || Value(ptg->getEndCst(i,j)) < end)){
				storage.push_back(Transition(i, j, ptg->getTransition(i,j)));
				ptg->setTransition(i,j,-1);
			}
		}
	}
}

void PTGSolver::restoreAllTrans(){
	while(!storage.empty()){
		ptg->setTransition(storage.front().origin, storage.front().dest,storage.front().cost);
		storage.pop_front();
	}
}

void PTGSolver::updateBottoms(){
	//Update the bottom transitions
	//cout << "updatebottoms" << endl;
	for (unsigned int i = 0; i < size; ++i){
		bottoms[i] = vals[i];
		//cout << bottoms[i] << " " << vals[i] << endl;
	}
}

void PTGSolver::rescale(Value start, Value end){
	//The result given by the solveSPTG
	cout << "====Rescaling====" << endl;
	for (vector<list<Point> >::iterator it = valueFcts.begin(); it != valueFcts.end(); ++it){
		for (list<Point>::iterator itL = it->begin(); itL != it->end() && itL->getX() <= 1 ; ++itL){
			itL->setX((itL->getX() * (end.getVal() - start.getVal())) + start);
		}
	}
	for (list<Strategy>::iterator it = strategies.begin(); it != strategies.end() && it->getTime() < 1; ++it)
		it->setTime((it->getTime() * (end.getVal() - start.getVal())) + start);
}

void PTGSolver::cleanValueFcts(){
	//In the case where three consecutive points are on the same line, we can delete the middle one
	cout << "====Cleaning Value Fcts====" << endl;

	for (vector<list<Point> >::iterator itV = valueFcts.begin(); itV != valueFcts.end(); ++itV){
		//For every state
		list<Point>::iterator itNext = itV->begin();
		++itNext;
		if(itNext != itV->end()){
			list<Point>::iterator itCurrent = itV->begin();
			++itNext;
			++itCurrent;
			for (list<Point>::iterator itLast = itV->begin(); itNext != itV->end(); ++itNext){
				bool deleted = false;
				if(!itLast->getY().isInfinity() && !itCurrent->getY().isInfinity() && !itNext->getY().isInfinity() && (itLast->getX().getVal() <= itCurrent->getX().getVal()) && (itCurrent->getX().getVal() <= itNext->getX().getVal())){
					Value coef = (itNext->getY() - itLast->getY())/(itNext->getX() - itLast->getX());
					if(itLast->getY() + (coef * (itCurrent->getX() - itLast->getX())) == itCurrent->getY()){
						itV->erase(itCurrent);
						deleted = true;
					}
				}
				else if(itLast->getY().isInfinity() && itCurrent->getY().isInfinity() && itNext->getY().isInfinity()&& (itLast->getX().getVal() <= itCurrent->getX().getVal()) && (itCurrent->getX().getVal() <= itNext->getX().getVal())){
					itV->erase(itCurrent);
					deleted = true;
				}
				if(deleted){
					//itCurrent is on a space that has been deleted so we can't do ++itCurrent and itLast needs to stay where it was
					itCurrent = itNext;
				}
				else{
					++itCurrent;
					++itLast;
				}
			}
		}
	}
}

void PTGSolver::correctStrats(){
	cout << "====Correction of the strategies====" << endl;
	for (list<Strategy>::reverse_iterator rit = strategies.rbegin(); rit != strategies.rend(); ++rit){
		list<Strategy>::reverse_iterator ritPrev = rit;
		++ritPrev;
		if(ritPrev != strategies.rend()){
			for (unsigned int i = 0; i < size; ++i){
				if(ritPrev->getType(i) == 1 || ritPrev->getType(i) == 2){
					ritPrev->setDest(i, rit->getDest(i));
				}
			}
		}
	}
}

void PTGSolver::cleanStrats(){
	//For every state
	list<Strategy>::iterator itNext = strategies.begin();
	++itNext;
	if(itNext != strategies.end()){
		list<Strategy>::iterator itCurrent = strategies.begin();
		++itNext;
		++itCurrent;
		for (list<Strategy>::iterator itLast = strategies.begin(); itNext != strategies.end(); ++itNext){


			bool deleted = false;
			bool equal = true;
			for (unsigned int i = 0; equal &&  i < size; ++i){
				if(itNext->getDest(i) != itCurrent->getDest(i) || itCurrent->getDest(i) != itLast->getDest(i) || itNext->getDest(i) != itLast->getDest(i)
						|| itNext->getType(i) != itCurrent->getType(i) || itCurrent->getType(i) != itLast->getType(i) || itNext->getType(i) != itLast->getType(i))
					equal = false;
			}
			if(equal){
				strategies.erase(itCurrent);
				deleted = true;
			}

			if(deleted){
				//itCurrent is on a space that has been deleted so we can't do ++itCurrent and itLast needs to stay where it was
				itCurrent = itNext;
			}
			else{
				++itCurrent;
				++itLast;
			}
		}
	}

}

void PTGSolver::createResets(){
	for (unsigned int i = 0; i < ptg->getSize(); ++i){
		resets.push_back(vector<CompositeValue>());
		for (unsigned int j = 0; j < ptg->getSize(); ++j){
			if(ptg->getReset(i,j)){
				resets[i].push_back(CompositeValue());
				resets[i][j].setInf(true);
			}
			else{
				resets[i].push_back(CompositeValue(-1));
			}
		}
	}
}

void PTGSolver::updateResets(){
	cout << "===Update resets===" << endl;

	//The cost of the reset transition is the sum between the cost of the transition and the value of the destination
	for (unsigned int i = 0; i < size; ++i){
		for (unsigned int j = 0; j < size; ++j){
			if(ptg->getReset(i,j)){
				//cout << i << " " << j << " " << ptg->getTransition(i,j) << " " << vals[j][0] << endl;
				resets[i][j] = vals[j] + ptg->getTransition(i,j);
			}
		}
	}
}

void PTGSolver::show(){

	/*cout << "bottoms:" << endl;
	for (unsigned int i = 0; i < bottoms.size(); ++i){
		cout <<  bottoms[i]<< "	";
	}
	cout << endl;*/


	cout << "Strategies: " << endl;
	for (list<Strategy>::iterator it = strategies.begin(); it != strategies.end(); ++it){
		it->show();
	}
	cout << endl;

	/*cout << "Lengths: " << endl;
	for (unsigned int i = 0; i < pathsLengths.size(); ++i)
		cout << pathsLengths[i] << "	";
	cout << endl;*/

	cout << "Vals:" << endl;
	for (unsigned int i = 0; i < vals.size(); ++i){
		cout << vals[i] << "	";
	}
	cout << endl;


	cout << "Value Functions" << endl;
	for (unsigned int i = 1; i < valueFcts.size(); ++i){
		cout << " State " << i <<": ";
		for(list<Point>::iterator it = valueFcts[i].begin(); it != valueFcts[i].end(); ++it){
			cout << "(" << it->getX() << "," << it->getY() << ") ";
		}
		cout << endl;
	}
}

void PTGSolver::visualize(){
	ofstream f;
	f.open("valueFcts.tex");
	f << "\\documentclass{standalone}" << endl;
	f << "\\usepackage{tikz}" << endl;
	f << "\\begin{document}" << endl;
	f << "\\begin{tikzpicture}" << endl;

	Fraction y = 0;
	const int length = 3;
	for (unsigned int i = 1; i < size; ++i, y = y - (length + 4) ){
		Fraction x = 0;
		Fraction maxY = 0;
		Fraction maxX = valueFcts[i].back().getX().getVal();

		for (list<Point>::iterator it = valueFcts[i].begin(); it != valueFcts[i].end(); ++it){
			if (!it->getY().isInfinity() && it->getY().getVal() > maxY){
				maxY = it->getY().getVal();
			}
		}

		if(maxY == 0)
			maxY = 1;

		//Draw the x axis
		f << "\\draw [->][thick] (0," << Fraction(0) - y << ") -- ( " << (ptg->getNbResets() + 2)* length  << "," << Fraction(0) - y << ");" << endl;
		f << "\\draw ( " << (ptg->getNbResets() + 2)* length << "," << Fraction(0) - y << ") node[right] {$t$};" << endl;
		//Draw the y axis
		f << "\\draw [->][thick] (0," << Fraction(0) - y << ") -- (0," << Fraction(length + 2) - y  << ");" << endl;
		f<< "\\draw (0," << Fraction(length + 2) - y<<") node[above] {$v_" <<  i << "(t)$}; " << endl;


		list<Point>::iterator itNext = valueFcts[i].begin();
		list<Point>::iterator itLast;
		list<Strategy>::iterator itStrat = strategies.begin();


		for(list<Point>::iterator it = valueFcts[i].begin(); it != valueFcts[i].end() && itStrat != strategies.end(); ++it){
			++itNext;

			if(itStrat->getInclusion() && (it->getX().getVal() == 0 || it->getX().getVal() == maxX || (itLast->getY() != it->getY() && itLast->getX() == it->getX()) || (itNext->getY() != it->getY() && itNext->getX() == it->getX()))){

				if(it->getY().isInfinity())
					f << "\\node [circle,draw,fill=black,scale=0.4] at (" << it->getX().getVal()/Fraction(maxX) * length + x << "," << Fraction(length + 1)  - y  << ") {};" << endl;
				else
					f << "\\node [circle,draw,fill=black,scale=0.4] at (" << it->getX().getVal()/Fraction(maxX) * length + x << "," << it->getY().getVal()/ Fraction(maxY) * Fraction(length)  - y  << ") {};" << endl;
			}
			else if (!itStrat->getInclusion() && ((itNext->getX() == it->getX() && itNext->getY() != it->getY()) || (itLast->getX() == it->getX() && itLast->getY() != it->getY()))){
				if(it->getY().isInfinity())
					f << "\\node [circle,draw,fill=white,scale=0.4] at (" << it->getX().getVal()/Fraction(maxX) * length + x << "," << Fraction(length + 1)  - y << ") {};" << endl;
				else
					f << "\\node [circle,draw,fill=white,scale=0.4] at (" << it->getX().getVal()/Fraction(maxX) * length + x << "," << it->getY().getVal()/ Fraction(maxY) * Fraction(length)  - y << ") {};" << endl;
			}

			if(itNext != valueFcts[i].end() && it->getX().getVal() != maxX && (!itStrat->getInclusion() || (itStrat->getInclusion() && it->getX() != itNext->getX()))  && itNext->getX() != it->getX()){
				if(it->getY().isInfinity())
					f << "\\draw [color=gray!100](" << it->getX().getVal()/Fraction(maxX) * length + x << "," << Fraction(length + 1)  - y << ") -- (" << itNext->getX().getVal()/Fraction(maxX) * length + x << "," << Fraction(length + 1)  - y << ");" << endl;
				else
					f << "\\draw [color=gray!100](" << it->getX().getVal()/Fraction(maxX) * length + x << "," << it->getY().getVal()/ Fraction(maxY) * Fraction(length)  - y << ") -- (" << itNext->getX().getVal()/Fraction(maxX) * length + x << "," << itNext->getY().getVal()/ Fraction(maxY) * Fraction(length)  - y << ");" << endl;

			}

			if(it->getY().isInfinity())
				f << "\\draw (0,"  << Fraction(length + 1) - y << ") node[left] {\\footnotesize$inf$};" << endl;
			else
				f << "\\draw (0,"  << it->getY().getVal()/Fraction(maxY)* length - y << ") node[left] {\\footnotesize$" << it->getY().getVal() << "$};" << endl;
			f << "\\draw ("  << it->getX().getVal()/Fraction(maxX)* length + x << "," << Fraction(0) - y << ") node[below] {\\footnotesize$" << it->getX().getVal() << "$};" << endl;


			if(it->getX().getVal() == maxX && itStrat->getInclusion()){
				x = x + length + 1;
			}
			itLast = it;
			++itStrat;

		}


	}

	f << "\\end{tikzpicture}" << endl;
	f << "\\end{document}" << endl;
	f.close();
	system("pdflatex valueFcts.tex");

	f.open("strategies.tex");
	f << "\\documentclass{standalone}" << endl;
	f << "\\usepackage{tikz}" << endl;
	f << "\\begin{document}" << endl;
	f << "\\begin{tikzpicture}" << endl;

	y = 0;

	for(unsigned int i = 1; i < size; ++i){
		cout << "State " << i << endl;
		Fraction x = 0;
		Fraction maxX = strategies.back().getTime().getVal();
		unsigned int maxY = size;
		for (list<Strategy>::iterator it = strategies.begin(); it != strategies.end() ; ++it){

			if(it->getTime().getVal() == 0 && it->getInclusion() && it != strategies.begin()){
				x = x + length + 1;
			}
			string color;
			if(it->getType(i) == 0)
				color = "black";
			else if(it->getType(i) == 1)
				color = "blue";
			else if(it->getType(i) == 2)
				color = "green";
			else if(it->getType(i) == 3)
				color = "red";

			list<Strategy>::iterator itNext = it;
			++itNext;

			string colorNext;
			if(itNext != strategies.end() && itNext->getType(i) == 0)
				colorNext = "black";
			else if(itNext != strategies.end() && itNext->getType(i) == 1)
				colorNext = "blue";
			else if(itNext != strategies.end() && itNext->getType(i) == 2)
				colorNext = "green";
			else if(itNext != strategies.end() && itNext->getType(i) == 3)
				colorNext = "red";


			//Draw the line
			if(itNext != strategies.end() && itNext->getTime().getVal() != 0){
				f << "\\draw [" << color << "] (" << it->getTime().getVal()/ maxX * Fraction(length) + x << "," << (Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length))  - y + 1<< ") -- (" << itNext->getTime().getVal()/maxX  * Fraction(length) + x<< "," << Fraction(it->getDest(i)) / Fraction(maxY) * Fraction(length) - y + 1<< ");" << endl;
				f << "\\draw (" << it->getTime().getVal() / maxX * Fraction(length) + x<< "," << Fraction (0) - y <<") node[below] {\\footnotesize$" << it->getTime().getVal() << "$};" << endl;
				f << "\\draw (0,"  << (Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length)) - y + 1 << ") node[left] {\\footnotesize$" << it->getDest(i) << "$};" << endl;
			}

			if(it->getInclusion() && ((itNext != strategies.end() && itNext->getDest(i) != it->getDest(i)) || it->getTime().getVal() == 0 || it->getTime().getVal() == maxX)){
				f << "\\node [circle,draw,fill= " << color <<",scale=0.4] at (" << it->getTime().getVal()/ maxX * Fraction(length) + x << "," << Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length)  - y + 1 << ") {};" << endl;
				if(!itNext->getInclusion() && itNext != strategies.end() && itNext->getDest(i) != it->getDest(i)){
					f << "\\draw[" << colorNext << ",fill=white] (" << itNext->getTime().getVal()/ maxX * Fraction(length) + x << "," << Fraction(itNext->getDest(i))/ Fraction(maxY) * Fraction(length)  - y + 1 << ") circle (0.07);" << endl;
					f << "\\draw (0,"  << (Fraction(itNext->getDest(i))/ Fraction(maxY) * Fraction(length)) - y + 1 << ") node[left] {\\footnotesize$" << itNext->getDest(i) << "$};" << endl;

				}

			}
			else if(!it->getInclusion() && itNext != strategies.end() && itNext->getDest(i) != it->getDest(i)){
				f << "\\draw[" << color << ",fill=white] (" << itNext->getTime().getVal()/ maxX * Fraction(length) + x << "," << Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length)  - y + 1 << ") circle (0.07);" << endl;
				cout << "time: " << itNext->getTime().getVal() << endl;
				if(itNext != strategies.end() && itNext->getDest(i) != it->getDest(i)){
					f << "\\draw (0,"  << (Fraction(itNext->getDest(i))/ Fraction(maxY) * Fraction(length)) - y + 1 << ") node[left] {\\footnotesize$" << itNext->getDest(i) << "$};" << endl;
					f << "\\draw[fill=" << colorNext << "] (" << itNext->getTime().getVal()/ maxX * Fraction(length) + x << "," << Fraction(itNext->getDest(i))/ Fraction(maxY) * Fraction(length)  - y + 1 << ") circle (0.07);" << endl;
				}
			}
			else if(it->getInclusion() && it->getTime().getVal() == 0){
				f << "\\draw[fill=" << color << "] (" << it->getTime().getVal()/ maxX * Fraction(length) + x << "," << Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length)  - y + 1 << ") circle (0.07);" << endl;
				f << "\\draw (0,"  << (Fraction(it->getDest(i))/ Fraction(maxY) * Fraction(length)) - y + 1 << ") node[left] {\\footnotesize$" << it->getDest(i) << "$};" << endl;

			}


		}

		//Draw the x axis
		f << "\\draw [->][thick] (0," << Fraction(0) - y << ") -- ( " << x + length + 1 << "," << Fraction(0) - y << ");" << endl;
		f << "\\draw ( " << x + length + 1 << "," << Fraction(0) - y << ") node[right] {$t$};" << endl;
		f << "\\draw (0," << Fraction(0) - y <<") node [below]{\\footnotesize$0$};" << endl;
		//Draw the y axis
		f << "\\draw [->][thick] (0," << Fraction(0) - y << ") -- (0," << Fraction(length + 1) - y  << ");" << endl;
		f<< "\\draw (0," << Fraction(length + 1) - y<<") node[above] {$\\sigma_" <<  i << "(t)$}; " << endl;

		y = y + 5;
	}

	f << "\\draw[black, fill=black] (0," << Fraction(3) - y << ") circle (0.07);" << endl;
	f << "\\node [right] at (0.5, " << Fraction(3) - y << ") {Normal transition};" << endl;
	f << "\\draw[black, fill=blue] (0," << Fraction(2) - y << ") circle (0.07);" << endl;
	f << "\\node [right] at (0.5, " << Fraction(2) - y << ") {Waiting (lambda transition)};" << endl;
	f << "\\draw[black, fill=green] (0," << Fraction(1) - y << ") circle (0.07);" << endl;
	f << "\\node [right] at (0.5, " << Fraction(1) - y << ") {Waiting (bottom transition)};" << endl;
	f << "\\draw[black, fill=red] (0," << Fraction(0) - y << ") circle (0.07);" << endl;
	f << "\\node [right] at (0.5, " << Fraction(0) - y << ") {Reset};" << endl;

	f << "\\end{tikzpicture}" << endl;
	f << "\\end{document}" << endl;
	system("pdflatex strategies.tex");


}


vector<list<Point> >* PTGSolver::getValueFcts(){
	return &valueFcts;
}
