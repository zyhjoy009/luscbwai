#include "Common.h"
#include "StrategyManager.h"

// constructor
StrategyManager::StrategyManager() 
	: firstAttackSent(false)
	, currentStrategy(0)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	addStrategies();
	setStrategy();
	ColorGraph::Instance();
}

// get an instance of this
StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
}

void StrategyManager::addStrategies()
{
	protossOpeningBook = std::vector<std::string>(NumProtossStrategies);
	terranOpeningBook  = std::vector<std::string>(NumTerranStrategies);
	zergOpeningBook    = std::vector<std::string>(NumZergStrategies);

	protossOpeningBook[ProtossZealotRush]	= "0 0 0 0 1 0 0 3 0 0 3 0 1 3 0 4 4 4 4 4 1 0 4 4 4";
	protossOpeningBook[ProtossDarkTemplar]	= "0 0 0 0 1 3 0 7 5 0 0 12 3 13 0 22 22 0 0";
	protossOpeningBook[ProtossDragoons]		= "0 0 0 0 1 0 0 3 0 7 0 0 5 0 0 3 8 6 1 6 6 0 3 1 0 6 6 6";
	terranOpeningBook[TerranMarineRush]		= "0 0 0 0 0 1 0 0 3 0 0 3 0 1 0 4 0 0 0 6";
	zergOpeningBook[ZergZerglingRush]		= "0 0 0 0 0 1 0 0 0 2 3 5 0 0 0 0 0 0 1 6";

	if (selfRace == BWAPI::Races::Protoss)
	{
		results = std::vector<IntPair>(NumProtossStrategies);

		if (enemyRace == BWAPI::Races::Protoss)
		{
			usableStrategies.push_back(ProtossZealotRush);
			usableStrategies.push_back(ProtossDarkTemplar);
			usableStrategies.push_back(ProtossDragoons);
		}
		else if (enemyRace == BWAPI::Races::Terran)
		{
			usableStrategies.push_back(ProtossZealotRush);
			usableStrategies.push_back(ProtossDarkTemplar);
			usableStrategies.push_back(ProtossDragoons);
		}
		else if (enemyRace == BWAPI::Races::Zerg)
		{
			usableStrategies.push_back(ProtossZealotRush);
			usableStrategies.push_back(ProtossDragoons);
		}
		else
		{
			BWAPI::Broodwar->printf("Enemy Race Unknown");
			usableStrategies.push_back(ProtossZealotRush);
			usableStrategies.push_back(ProtossDragoons);
		}
	}
	else if (selfRace == BWAPI::Races::Terran)
	{
		results = std::vector<IntPair>(NumTerranStrategies);
		usableStrategies.push_back(TerranMarineRush);
	}
	else if (selfRace == BWAPI::Races::Zerg)
	{
		results = std::vector<IntPair>(NumZergStrategies);
		usableStrategies.push_back(ZergZerglingRush);
	}

	if (Options::Modules::USING_STRATEGY_IO)
	{
		readResults();
	}
}

void StrategyManager::readResults()
{
	// read in the name of the read and write directories from settings file
	struct stat buf;

	// if the file doesn't exist something is wrong so just set them to default settings
	if (stat(Options::FileIO::FILE_SETTINGS, &buf) == -1)
	{
		readDir = "bwapi-data/testio/read/";
		writeDir = "bwapi-data/testio/write/";
	}
	else
	{
		std::ifstream f_in(Options::FileIO::FILE_SETTINGS);
		getline(f_in, readDir);
		getline(f_in, writeDir);
		f_in.close();
	}

	// the file corresponding to the enemy's previous results
	std::string readFile = readDir + BWAPI::Broodwar->enemy()->getName() + ".txt";

	// if the file doesn't exist, set the results to zeros
	if (stat(readFile.c_str(), &buf) == -1)
	{
		std::fill(results.begin(), results.end(), IntPair(0,0));
	}
	// otherwise read in the results
	else
	{
		std::ifstream f_in(readFile.c_str());
		std::string line;
		getline(f_in, line);
		results[ProtossZealotRush].first = atoi(line.c_str());
		getline(f_in, line);
		results[ProtossZealotRush].second = atoi(line.c_str());
		getline(f_in, line);
		results[ProtossDarkTemplar].first = atoi(line.c_str());
		getline(f_in, line);
		results[ProtossDarkTemplar].second = atoi(line.c_str());
		getline(f_in, line);
		results[ProtossDragoons].first = atoi(line.c_str());
		getline(f_in, line);
		results[ProtossDragoons].second = atoi(line.c_str());
		f_in.close();
	}

	BWAPI::Broodwar->printf("Results (%s): (%d %d) (%d %d) (%d %d)", BWAPI::Broodwar->enemy()->getName().c_str(), 
		results[0].first, results[0].second, results[1].first, results[1].second, results[2].first, results[2].second);
}

void StrategyManager::writeResults()
{
	std::string writeFile = writeDir + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::ofstream f_out(writeFile.c_str());

	f_out << results[ProtossZealotRush].first   << "\n";
	f_out << results[ProtossZealotRush].second  << "\n";
	f_out << results[ProtossDarkTemplar].first  << "\n";
	f_out << results[ProtossDarkTemplar].second << "\n";
	f_out << results[ProtossDragoons].first     << "\n";
	f_out << results[ProtossDragoons].second    << "\n";

	f_out.close();
}

void StrategyManager::setStrategy()
{
	// if we are using file io to determine strategy, do so
	if (Options::Modules::USING_STRATEGY_IO)
	{
		double bestUCB = -1;
		int bestStrategyIndex = 0;

		// UCB requires us to try everything once before using the formula
		for (size_t strategyIndex(0); strategyIndex<usableStrategies.size(); ++strategyIndex)
		{
			int sum = results[usableStrategies[strategyIndex]].first + results[usableStrategies[strategyIndex]].second;

			if (sum == 0)
			{
				currentStrategy = usableStrategies[strategyIndex];
				return;
			}
		}

		// if we have tried everything once, set the maximizing ucb value
		for (size_t strategyIndex(0); strategyIndex<usableStrategies.size(); ++strategyIndex)
		{
			double ucb = getUCBValue(usableStrategies[strategyIndex]);

			if (ucb > bestUCB)
			{
				bestUCB = ucb;
				bestStrategyIndex = strategyIndex;
			}
		}
		
		currentStrategy = usableStrategies[bestStrategyIndex];
	}
	else
	{
		// otherwise return a random strategy
		currentStrategy = ProtossZealotRush;
	}

}

void StrategyManager::onEnd(const bool isWinner)
{
	// write the win/loss data to file if we're using IO
	if (Options::Modules::USING_STRATEGY_IO)
	{
		// if the game ended before the tournament time limit
		if (BWAPI::Broodwar->getFrameCount() < Options::Tournament::GAME_END_FRAME)
		{
			if (isWinner)
			{
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
			}
		}
		// otherwise game timed out so use in-game score
		else
		{
			if (getScore(BWAPI::Broodwar->self()) > getScore(BWAPI::Broodwar->enemy()))
			{
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
			}
		}
		
		writeResults();
	}
}

const double StrategyManager::getUCBValue(const size_t & strategy) const
{
	double totalTrials(0);
	for (size_t s(0); s<usableStrategies.size(); ++s)
	{
		totalTrials += results[usableStrategies[s]].first + results[usableStrategies[s]].second;
	}

	double C		= 0.7;
	double wins		= results[strategy].first;
	double trials	= results[strategy].first + results[strategy].second;

	double ucb = (wins / trials) + C * sqrt(std::log(totalTrials) / trials);

	return ucb;
}

const int StrategyManager::getScore(BWAPI::Player * player) const
{
	return player->getBuildingScore() + player->getKillScore() + player->getRazingScore() + player->getUnitScore();
}

const std::string StrategyManager::getOpeningBook() const
{
	if (selfRace == BWAPI::Races::Protoss)
	{
		return protossOpeningBook[currentStrategy];
	}
	else if (selfRace == BWAPI::Races::Terran)
	{
		return terranOpeningBook[currentStrategy];
	}
	else if (selfRace == BWAPI::Races::Zerg)
	{
		return zergOpeningBook[currentStrategy];
	} 

	// something wrong, return the protoss one
	return protossOpeningBook[currentStrategy];
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
const bool StrategyManager::defendWithWorkers()
{
	if (!Options::Micro::WORKER_DEFENSE)
	{
		return false;
	}

	// our home nexus position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of nexus
	int defenseRadius = 300;

	// fill the set with the types of units we're concerned about
	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a zergling or a worker we want to defend
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling || unit->getType().isWorker())
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers > 0;
}

// called by combat commander to determine whether or not to send an attack force
// freeUnits are the units available to do this attack
const bool StrategyManager::doAttack(const std::set<BWAPI::Unit *> & freeUnits)
{
	int ourForceSize = (int)freeUnits.size();

	int numUnitsNeededForAttack = 3;

	bool doAttack  = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) >= 1
					|| ourForceSize >= numUnitsNeededForAttack;

	if (doAttack)
	{
		firstAttackSent = true;
	}

	return doAttack || firstAttackSent;
}

const bool StrategyManager::expandProtossZealotRush() const
{
	// if there is no place to expand to, we can't expand
	if (MapTools::Instance().getNextExpansion() == BWAPI::TilePositions::None)
	{
		return false;
	}

	int numNexus =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numZealots =			BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int frame =					BWAPI::Broodwar->getFrameCount();

	// if there are more than 10 idle workers, expand
	if (WorkerManager::Instance().getNumIdleWorkers() > 10)
	{
		return true;
	}

	// 2nd Nexus Conditions:
	//		We have 12 or more zealots
	//		It is past frame 7000
	if ((numNexus < 2) && (numZealots > 12 || frame > 9000))
	{
		return true;
	}

	// 3nd Nexus Conditions:
	//		We have 24 or more zealots
	//		It is past frame 12000
	if ((numNexus < 3) && (numZealots > 24 || frame > 15000))
	{
		return true;
	}

	if ((numNexus < 4) && (numZealots > 24 || frame > 21000))
	{
		return true;
	}

	if ((numNexus < 5) && (numZealots > 24 || frame > 26000))
	{
		return true;
	}

	if ((numNexus < 6) && (numZealots > 24 || frame > 30000))
	{
		return true;
	}

	return false;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
	{
		if (getCurrentStrategy() == ProtossZealotRush)
		{
			return getProtossZealotRushBuildOrderGoal();
		}
		else if (getCurrentStrategy() == ProtossDarkTemplar)
		{
			return getProtossDarkTemplarBuildOrderGoal();
		}
		else if (getCurrentStrategy() == ProtossDragoons)
		{
			return getProtossDragoonsBuildOrderGoal();
		}

		// if something goes wrong, use zealot goal
		return getProtossZealotRushBuildOrderGoal();
	}
	else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
	{		
		return getTerranBuildOrderGoal();
	}
	else
	{
		return getZergBuildOrderGoal();
	}
}

const MetaPairVector StrategyManager::getProtossDragoonsBuildOrderGoal() const
{
		// the goal to return
	MetaPairVector goal;

	int numDragoons =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numProbes =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Probe);
	int numNexusCompleted =		BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numCyber =				BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	int numCannon =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);

	int dragoonsWanted = numDragoons > 0 ? numDragoons + 6 : 2;
	int gatewayWanted = 3;
	int probesWanted = numProbes + 6;

	if (InformationManager::Instance().enemyHasCloakedUnits())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));
	
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
		}
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
		}
	}
	else
	{
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
		}

		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
		}
	}

	if (numNexusAll >= 2 || numDragoons > 6 || BWAPI::Broodwar->getFrameCount() > 9000)
	{
		gatewayWanted = 6;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));
	}

	if (numNexusCompleted >= 3)
	{
		gatewayWanted = 8;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 2));
	}

	if (numNexusAll > 1)
	{
		probesWanted = numProbes + 6;
	}

	if (expandProtossZealotRush())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	}

	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon,	dragoonsWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway,	gatewayWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Probe,	std::min(90, probesWanted)));

	return goal;
}

const MetaPairVector StrategyManager::getProtossDarkTemplarBuildOrderGoal() const
{
	// the goal to return
	MetaPairVector goal;

	int numDarkTeplar =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
	int numDragoons =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numProbes =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Probe);
	int numNexusCompleted =		BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numCyber =				BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	int numCannon =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);

	int darkTemplarWanted = 0;
	int dragoonsWanted = numDragoons + 6;
	int gatewayWanted = 3;
	int probesWanted = numProbes + 6;

	if (InformationManager::Instance().enemyHasCloakedUnits())
	{
		
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));
		
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
		}
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
		}
	}

	if (numNexusAll >= 2 || BWAPI::Broodwar->getFrameCount() > 9000)
	{
		gatewayWanted = 6;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));
	}

	if (numDragoons > 0)
	{
		goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
	}

	if (numNexusCompleted >= 3)
	{
		gatewayWanted = 8;
		dragoonsWanted = numDragoons + 6;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
	}

	if (numNexusAll > 1)
	{
		probesWanted = numProbes + 6;
	}

	if (expandProtossZealotRush())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	}

	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon,	dragoonsWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway,	gatewayWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, darkTemplarWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Probe,	std::min(90, probesWanted)));
	
	return goal;
}

const MetaPairVector StrategyManager::getProtossZealotRushBuildOrderGoal() const
{
	// the goal to return
	MetaPairVector goal;

	int numZealots =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int numDragoons =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numProbes =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Probe);
	int numNexusCompleted =		BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numCyber =				BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	int numCannon =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);

	int zealotsWanted = numZealots + 8;
	int dragoonsWanted = numDragoons;
	int gatewayWanted = 3;
	int probesWanted = numProbes + 4;

	if (InformationManager::Instance().enemyHasCloakedUnits())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));
		
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
		}
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
		}
	}

	if (numNexusAll >= 2 || BWAPI::Broodwar->getFrameCount() > 9000)
	{
		gatewayWanted = 6;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Assimilator, 1));
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
	}

	if (numCyber > 0)
	{
		dragoonsWanted = numDragoons + 2;
		goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
	}

	if (numNexusCompleted >= 3)
	{
		gatewayWanted = 8;
		dragoonsWanted = numDragoons + 6;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
	}

	if (numNexusAll > 1)
	{
		probesWanted = numProbes + 6;
	}

	if (expandProtossZealotRush())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	}

	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon,	dragoonsWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot,	zealotsWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway,	gatewayWanted));
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Probe,	std::min(90, probesWanted)));

	return goal;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal() const
{
	// the goal to return
	std::vector< std::pair<MetaType, UnitCountType> > goal;

	std::map<BWAPI::UnitType, int> unitCounts;
	
	// count up the number of units we have, and are training and constructing
		// NOTE: in previous versions, we used BWAPI::Broodwar->self()->allUnitCount() and BWAPI::Broodwar->self()->completedUnitCount()
		// but both were found to have issues, so we just count them ourselves now
	BOOST_FOREACH(BWAPI::Unit *u, BWAPI::Broodwar->self()->getUnits())
	{
		// if the key isn't already in the array, initialize it to 0
		if(unitCounts.count(u->getType()) != 1)
			unitCounts[u->getType()] = 0;

		unitCounts[u->getType()]++;	

		// if this unit is a building training units, BWAPI does not account for any units further in the training queue than
		// the unit currently being trained. we must manually add in those units
		if(u->getType().isBuilding() && u->isTraining())
		{
			std::list<BWAPI::UnitType, std::allocator<BWAPI::UnitType>> queue = u->getTrainingQueue();

			bool skipped = false;
			BOOST_FOREACH(BWAPI::UnitType type, queue)
			{
				// skip the first unit, because BWAPI accounts for it
				if(!skipped)
				{
					skipped = true;
					continue;
				}

				if(unitCounts.count(type) != 1)
					unitCounts[type] = 0;

				unitCounts[type]++;	
			}
		}

	}
	
	/*

		We want the following units for our build order:

		- One vulture for every 5 tanks we are creating.
		- One goliath for every 5 tanks we are creating.
		- 5 more tanks + 33% more tanks.
		- At least 2 machine shops.
		- 5 missile turrets / command center.

	*/

	//==========================================
	//     UNITS
	//==========================================

	// include both types of tanks
	int numTanks = 0;
	numTanks += unitCounts.count(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode] : 0;
	numTanks += unitCounts.count(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode] : 0;

	int numVultures = unitCounts.count(BWAPI::UnitTypes::Terran_Vulture) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Vulture] : 0;	
	int vulturesWanted = numVultures + (numTanks / 4);
	if(vulturesWanted > 0)
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture,	vulturesWanted));

	int numGoliaths = unitCounts.count(BWAPI::UnitTypes::Terran_Goliath) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Goliath] : 0;
	int goliathsWanted = numGoliaths + (numTanks / 4);
	if(goliathsWanted > 0)
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath,	goliathsWanted));

	int tanksWanted = numTanks == 0 ? 2 : numTanks + numTanks / 3 + 7;
	goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode,	tanksWanted));			

	//==========================================
	//     BUILDINGS
	//==========================================

	int machineShops = unitCounts.count(BWAPI::UnitTypes::Terran_Machine_Shop) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Machine_Shop] : 0;
	int machineShopsWanted;
	// we want to have two machine shops, but only to build one at a time, otherwise the build order takes too long to calculate
	if(machineShops < 2)
		machineShopsWanted = machineShops + 1;
	// after that we don't need to force anymore
	else
		machineShopsWanted = 0;
	if(machineShopsWanted > 0)
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Machine_Shop,	machineShopsWanted));
	}

	int numCommSat = unitCounts.count(BWAPI::UnitTypes::Terran_Comsat_Station) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Comsat_Station] : 0;
	if(numTanks >= 5 && numCommSat < 1)
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Comsat_Station, 1));
	}

	int numBases = unitCounts.count(BWAPI::UnitTypes::Terran_Command_Center) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Command_Center] : 0;
	int numTurrets = unitCounts.count(BWAPI::UnitTypes::Terran_Missile_Turret) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_Missile_Turret] : 0;
	int turretsWanted = numTanks >= 0 ? numBases * 3 : 0;
	// make sure we don't try to make more than 3 at a time
	turretsWanted = turretsWanted - numTurrets > 3 ? numTurrets + 3 : turretsWanted;
	if(turretsWanted > numTurrets)
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Missile_Turret,	turretsWanted));

	// we don't want the tree to be too much to compute, so if our goal units add up to more than 25, just do a hard coded distribution
	int numberOfThingsToBuild = vulturesWanted - numVultures + tanksWanted - numTanks + goliathsWanted - numGoliaths + turretsWanted - numTurrets;
	if(numberOfThingsToBuild > 25)
	{
		if(turretsWanted - numTurrets > 0)
		{
			vulturesWanted = numVultures + 3;
			goliathsWanted = numGoliaths + 8;
			tanksWanted = numTanks + 15;
		}
		else
		{
			vulturesWanted = numVultures + 2;
			goliathsWanted = numGoliaths + 6;
			tanksWanted = numTanks + 13;
			turretsWanted = numTurrets + 5;
		}
	}

	// if our total units are over 198, just make it 198
	int numScvs = unitCounts.count(BWAPI::UnitTypes::Terran_SCV) == 1 ? unitCounts[BWAPI::UnitTypes::Terran_SCV] : 0;
	int totalUnitGoal = vulturesWanted + tanksWanted + goliathsWanted + numScvs;
	if(totalUnitGoal > 198)
	{		
		int totalMoreWanted = 198 - (numVultures + numTanks + numGoliaths + numScvs);
		vulturesWanted = numVultures + totalMoreWanted/3;
		goliathsWanted = numGoliaths + totalMoreWanted/3;
		tanksWanted = numTanks + totalMoreWanted/3;
	}

	//==========================================
	//     RESEARCH
	//==========================================

	if(numTanks > 0 && !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));		
		BWAPI::Broodwar->printf("Trying to research siege mode!");
	}

	
	if(BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Charon_Boosters) == 0 && numGoliaths > 0)
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Charon_Boosters, 1));
		BWAPI::Broodwar->printf("Trying to get charon boosters!");
	}


	BWAPI::Broodwar->printf("Getting %d vultures, %d tanks, %d goliaths, %d mach. shops, %d turrets", (vulturesWanted-numVultures), 
		(tanksWanted-numTanks), (goliathsWanted - numGoliaths), machineShopsWanted, (turretsWanted - numTurrets));

	return (const std::vector< std::pair<MetaType, UnitCountType> >)goal;
}

const MetaPairVector StrategyManager::getZergBuildOrderGoal() const
{
	// the goal to return
	std::vector< std::pair<MetaType, UnitCountType> > goal;
	
	int numMutas  =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Zerg_Mutalisk);
	int numHydras  =			BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk);

	int mutasWanted = numMutas + 6;
	int hydrasWanted = numHydras + 6;

	goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, 4));
	//goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Stim_Packs,	1));

	//goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic,		medicsWanted));

	return (const std::vector< std::pair<MetaType, UnitCountType> >)goal;
}

 const int StrategyManager::getCurrentStrategy()
 {
	 return currentStrategy;
 }