#include <SFML/Graphics.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <time.h>
#include <condition_variable>
#include <stack>
#include <set>
#include <future>

#include "macros.h"
#include "util.h"
#include "line.h"
#include "node.h"
#include "pcw.h"
#include "train.h"
#include "citizen.h"

int VALID_LINES;
int VALID_NODES;
int VALID_TRAINS;

float SIM_SPEED;
float tempSimSpeed;
const int TARGET_FPS = 60;
const std::chrono::microseconds FRAME_DURATION(1000000 / TARGET_FPS);

long long unsigned int simTick;
long unsigned int renderTick;

unsigned int totalRidership;
unsigned int handledCitizens;

std::vector<size_t> activeCitizensStat;
std::vector<double> clockStat;
std::vector<double> simSpeedStat;

Line lines[MAX_LINES];
Node nodes[MAX_NODES];
Train trains[MAX_TRAINS];
CitizenVector citizens(MAX_CITIZENS / 4, MAX_CITIZENS);

Line WALKING_LINE;

std::mutex trainsMutex;
std::mutex citizensMutex;
std::mutex pathsMutex;

std::atomic<bool> justDidPathfinding(false);
std::atomic<bool> shouldExit(false);

std::condition_variable doPathfinding;

// TODO "best-practice" everything into proper .h and .cpp files

bool addCitizen(Node* startNode, Node* endNode) {
	if (citizens.add(startNode, endNode)) {
		handledCitizens++;
		return true;
	}
	return false;
}

void generateCitizens(int spawnAmount) {
	int spawnedCount = 0;

	// int i = lastCitizenSpawnedIndex;
	while (spawnedCount < spawnAmount) {
		int startRidership = rand() % totalRidership;
		int endRidership;
		int startNode;
		int endNode;
		do {
			startNode = 0;
			endNode = 0;
			endRidership = rand() % totalRidership;
			// this calculation could technically be optimized, but what am I, a computer science student?
			int i = 0;
			while (i < startRidership) {
				i += nodes[startNode++].ridership;
			}
			i = 0;
			while (i < endRidership) {
				i += nodes[endNode++].ridership;
			}
		} while (endNode == startNode);

		// add citizen
		if (addCitizen(&nodes[startNode], &nodes[endNode])) {
			spawnedCount++;
		}
	}
}

int init() {
	// allocate and initialize global arrays
	for (int i = 0; i < MAX_LINES; i++) {
		lines[i] = Line();
	}
	std::cout << "Initialized lines" << std::endl;

	float nodesX[MAX_NODES]; // utility arrays for Node position normalization
	float nodesY[MAX_NODES];
	for (int i = 0; i < MAX_NODES; i++) {
		nodes[i] = Node();
		nodesX[i] = 0;
		nodesY[i] = 0;
	}
	std::cout << "Initialized nodes" << std::endl;

	for (int i = 0; i < MAX_TRAINS; i++) {
		trains[i] = Train();
	}
	std::cout << "Initialized trains" << std::endl;

	// read files
	int row = 0;
	std::string line;

	// parse [id (name), color, path] to generate Lines
	std::ifstream linesCSV("lines_stations.csv");
	if (!linesCSV.is_open()) {
		std::cerr << "Error opening lines_stations.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}
	std::cout << "Successfully opened lines_stations.csv" << std::endl;

	while (std::getline(linesCSV, line)) {
		std::stringstream lineStream(line);
		std::string cell;
		lines[row].size |= STATUS_SPAWNED;
		
		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			if (col == 0) {
				// line id/name (e.g. A, A_L, F)
				std::strcpy(lines[row].id, cell.c_str());
			} else if (col == 1) {
				// color (type sf::Color)
				colorConvert(&lines[row].color, cell);
			} else {
				// update Node in path
				lines[row].path[col - 2] = &nodes[std::stoi(cell)];
			}
			col++;
		}
		row++;
	}
	VALID_LINES = row;

	// parse [id (name), x, y, ridership] to generate Node objects
	std::ifstream stationsCSV("stations_data.csv");
	if (!stationsCSV.is_open()) {
		std::cerr << "Error opening stations_data.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}
	std::cout << "Successfully opened stations_data.csv" << std::endl;

	row = 0;
	totalRidership = 0;
	while (std::getline(stationsCSV, line)) {
		std::stringstream lineStream(line);
		std::string cell;
		nodes[row].status = STATUS_EXISTS;

		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			switch (col) {
			case 0: // numerical uid
				nodes[row].numerID = std::stoi(cell);
				break;
			case 1: // station id/name (e.g. Astor Pl, 23rd St)
				std::strcpy(nodes[row].id, cell.c_str());
				break;
			case 2: // x coordinate
				nodesX[row] = std::stof(cell);
				break;
			case 3: // y coordinate
				nodesY[row] = std::stof(cell);
				break;
			case 4: // can ignore lines value
				break;
			case 5: // ridership (daily, 2019)
				nodes[row].ridership = std::stoi(cell);
				totalRidership += nodes[row].ridership;
				break;
			default:
				break;
			}
			col++;
		}
		nodes[row] = nodes[row];
		row++;
	}
	VALID_NODES = row;
	std::cout << "Parsed stations data (total system ridership: " << totalRidership << ")" << std::endl;

	// normalize Node position data to screen boundaries
	float minNodeX = nodesX[0]; float maxNodeX = nodesX[0];
	float minNodeY = nodesY[0]; float maxNodeY = nodesY[0];
	for (int i = 0; i < VALID_NODES; i++) {
		if (nodesX[i] < minNodeX) minNodeX = nodesX[i];
		if (nodesX[i] > maxNodeX) maxNodeX = nodesX[i];
		if (nodesY[i] < minNodeY) minNodeY = nodesY[i];
		if (nodesY[i] > minNodeY) maxNodeY = nodesY[i];
	}
	float minMaxDiffX = maxNodeX - minNodeX;
	float minMaxDiffY = maxNodeY - minNodeY;
	for (int i = 0; i < VALID_NODES; i++) {
		nodes[i].setPosition(Vector2f(
			WINDOW_X_OFFSET + WINDOW_SCALE * WINDOW_X_SCALE * WINDOW_X * (nodesX[i] - minNodeX) / minMaxDiffX,
			WINDOW_Y_OFFSET + WINDOW_Y - WINDOW_SCALE * WINDOW_Y_SCALE * WINDOW_Y * (nodesY[i] - minNodeY) / minMaxDiffY
		));
	}
	std::cout << "Normalized node positions" << std::endl;

	// add Node transfer neighbors
	// TODO segment window into grid (subtree)
	WALKING_LINE = Line();
	std::strcpy(WALKING_LINE.id, WALK_LINE_ID_STR);

	int transferNeighbors = 0;
	for (int i = 0; i < VALID_NODES; i++) {
		for (int j = 0; j < VALID_NODES; j++) {
			if (i == j) continue;
			if (nodes[i].dist(&nodes[j]) <= TRANSFER_MAX_DIST) {
				struct PathWrapper neighborWrapper1 = { &nodes[i], &WALKING_LINE };
				struct PathWrapper neighborWrapper2 = { &nodes[j], &WALKING_LINE };
				float dist = nodes[i].dist(&nodes[j]);
				nodes[i].addNeighbor(&neighborWrapper2, dist);
				nodes[j].addNeighbor(&neighborWrapper1, dist);
				transferNeighbors++;
			}
		}
	}
	std::cout << "Generated node neighbors" << std::endl;

	// various preprocessing steps, generate Train objects
	VALID_TRAINS = 0;
	int lineNeighbors = 0;

	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		Line* line = &lines[i];

		// update Node colors for each Line
		while (line->path[j] != NULL && line->path[j]->status == STATUS_EXISTS) {
			if (j > 0) {
				line->dist[j - 1] = line->path[j]->dist(line->path[j - 1]) * 128;
				struct PathWrapper neighborWrapper1 = { line->path[j], line };
				struct PathWrapper neighborWrapper2 = { line->path[j - 1], line };
				float dist = line->path[j]->dist(line->path[j - 1]) * TRANSFER_PENALTY_MULTIPLIER;
				line->path[j]->addNeighbor(&neighborWrapper2, dist);
				line->path[j - 1]->addNeighbor(&neighborWrapper1, dist);
				lineNeighbors++;
			}
			line->path[j]->setFillColor(sf::Color(line->color.r, line->color.g, line->color.b));
			j++;
		}

		// generate distances between Nodes on each Line
		line->dist[j - 1] = line->dist[j-2];

		// update Line size (length)
		line->size = j;

		// generate Train objects
		for (int k = 0; k < j; k+= DEFAULT_TRAIN_STOP_SPACING) {
			// generate 2 Trains (one going backward, one forward) except if at first/last stop
			int repeat = (k == 0 || k == j - 1) ? 1 : 2;
			for (int l = 0; l < repeat; l++) {
				Train* train = &trains[VALID_TRAINS];
				train->setPosition(line->path[k]->getPosition());
				train->line = line;
				train->index = k;
				train->status = STATUS_IN_TRANSIT;
				train->statusForward = (l == 1) ? STATUS_BACK : (k == j - 1) ? STATUS_BACK : STATUS_FORWARD;
				train->setFillColor(line->color);
				VALID_TRAINS++;
			}
		}
	}
	std::cout << "Processed nodes and generated initial trains" << std::endl;

	// tick counters used for timing various operations
	simTick = 0;
	renderTick = 0;

	// metrics used for debugging (especially in benchmark mode)
	handledCitizens = 0;

	// generate initial batch of citizens
	generateCitizens(CITIZEN_SPAWN_INIT);

	std::cout << std::endl << "INIT DONE!" << std::endl;

	std::cout << "Loaded " << VALID_NODES << " valid nodes on " << VALID_LINES << " lines" << std::endl;
	std::cout << "Initialized " << VALID_TRAINS << " trains" << std::endl;
	std::cout << "Generated " << transferNeighbors + lineNeighbors << " neighbor pairs (" << transferNeighbors << " transfer/" << lineNeighbors << " line)" << std::endl;
	std::cout << "Generated " << CITIZEN_SPAWN_INIT << " initial citizens" << std::endl << std::endl;

	return AOK;
}

void renderingThread() {
	// window
	sf::ContextSettings settings;
	settings.antialiasingLevel = 4;
	sf::RenderWindow window(sf::VideoMode(WINDOW_X, WINDOW_Y), "CitySim", sf::Style::Titlebar | sf::Style::Close, settings);

	// fps limiter
	sf::Clock clock;

	// white background
	sf::RectangleShape bg(Vector2f(WINDOW_X * 10, WINDOW_Y * 10));
	bg.setPosition(WINDOW_X * -5, WINDOW_Y * -5);
	bg.setFillColor(sf::Color(255, 255, 255, 255));

	// stats text
	sf::Text text;
	sf::Font font;
	font.loadFromFile("Arial.ttf");
	text.setFont(font);
	text.setCharacterSize(16);
	text.setFillColor(sf::Color::Black);

	// draw handlers
	sf::View view(Vector2f(WINDOW_X / 2, WINDOW_Y / 2), Vector2f(WINDOW_X, WINDOW_Y));
	Vector2f panOffset(0, 0);
	float simZoom = 1.0f;
	bool drawNodes = true;
	bool drawLines = true;
	bool drawTrains = true;
	bool drawCitizens = false;
	bool paused = false;

	// generate vertex buffer for line (path) shapes
	// copy line data to 1 dimensional vertex vector
	std::vector<sf::Vertex> lineVertices;
	lineVertices.reserve(VALID_NODES);
	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		while (lines[i].path[j] != nullptr && lines[i].path[j]->status == STATUS_EXISTS) {
			sf::Vector2f position = lines[i].path[j]->getPosition();
			sf::Color color = lines[i].path[j]->getFillColor();

			if (j != 0) lineVertices.push_back(sf::Vertex(position, color));
			lineVertices.push_back(sf::Vertex(position, color));
			j++;
		}
		if (!lineVertices.empty()) lineVertices.pop_back();
	}

	// copy vector data to buffer and clear leftovers
	sf::VertexBuffer linesVertexBuffer(sf::Lines, sf::VertexBuffer::Usage::Static);
	linesVertexBuffer.create(lineVertices.size());
	linesVertexBuffer.update(lineVertices.data());
	lineVertices.clear();
	lineVertices.shrink_to_fit();

	// initialize vertex array for nodes, trains
	// not sure if I should be using a VertexBuffer for these
	sf::VertexArray nodeVertices(sf::Triangles);
	sf::VertexArray trainVertices(sf::Triangles);

	float TRAIN_CAPACITY_FLOAT = float(TRAIN_CAPACITY);
	float NODE_CAPACITY_FLOAT = float(NODE_CAPACITY);

	simSpeedStat.push_back(0);

	// user path stuff
	Node* node1 = nullptr;
	Node* node2 = nullptr;
	char nodeCount = 0;
	PathWrapper userPath[CITIZEN_PATH_SIZE];
	char userPathSize;
	sf::VertexBuffer userPathVertexBuffer(sf::LinesStrip, sf::VertexBuffer::Usage::Static);

	while (window.isOpen() && !shouldExit) {
		renderTick++;

		// fps limiter
		sf::Time frameStart = clock.getElapsedTime();

		// get nearest node
		// TODO optimize with subtree
		float minDist = WINDOW_X * WINDOW_Y;
		Node* closestNode = nullptr;
		for (int i = 0; i < VALID_NODES; i++) {
			// TODO transform by zoom (REQUIRED!) -- write a function to get "real" mouse position
			Vector2f pos = Vector2f(sf::Mouse::getPosition() - window.getPosition()) - Vector2f(0, 40);
			float dist = nodes[i].dist(pos.x, pos.y);
			if (dist < minDist) {
				minDist = dist;
				closestNode = &nodes[i];
			}
		}

		// handle window events (including pan/zoom)
		sf::Event event;
		while (window.pollEvent(event))
		{
			// close
			if (event.type == sf::Event::Closed) {
				shouldExit = true;
				window.close();
			}
			// zoom
			else if (event.type == sf::Event::MouseWheelScrolled) {
				float zoom = 1.0f + event.mouseWheelScroll.delta * ZOOM_SCALE * -1;
				simZoom *= zoom;
				if (simZoom > ZOOM_MIN && simZoom < ZOOM_MAX) {
					view.zoom(zoom);
				}
				else {
					simZoom = std::max(ZOOM_MIN, std::min(simZoom, ZOOM_MAX));
				}
			}
			// pan
			else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
				panOffset = Vector2f(sf::Mouse::getPosition(window)) - view.getCenter();
			}
			else if (event.type == sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
				view.setCenter(Vector2f(sf::Mouse::getPosition(window)) - panOffset);
			}
			// draw custom paths by right clicking to select nearest node
			else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
				std::vector<sf::Vertex> userPathVertices;
				switch (nodeCount) {
				case 0:
					node1 = closestNode;
					std::cout << "User selected start " << node1->id << std::endl;
					nodeCount++;
					break;
				case 1:
					node2 = closestNode;
					if (node2->findPath(node1, userPath, &userPathSize)) {
						std::cout << "User selected end " << node2->id << std::endl;
						for (char i = 0; i < userPathSize; i++) {
							userPathVertices.push_back(sf::Vertex(userPath[i].node->getPosition(), userPath[i].node->getFillColor()));
						}
						userPathVertexBuffer.create(userPathSize);
						userPathVertexBuffer.update(userPathVertices.data());
						nodeCount++;
					}
					break;
				case 2:
					std::cout << "User cleared selection" << std::endl;
					userPathSize = 0;
					userPathVertices.clear();
					userPathVertexBuffer.update(userPathVertices.data());
					nodeCount = 0;
					break;
				}
			}
			// TODO pan with arrow keys
			else if (event.type == sf::Event::KeyPressed) {
				// press 1 to toggle nodes visibility
				if (event.key.code == sf::Keyboard::Num1) {
					drawNodes = !drawNodes;
				}
				// press 2 to toggle lines visibility
				if (event.key.code == sf::Keyboard::Num2) {
					drawLines = !drawLines;
				}
				// press 3 to toggle trains visibility
				if (event.key.code == sf::Keyboard::Num3) {
					drawTrains = !drawTrains;
				}
				// press 4 to toggle citizens visibility
				if (event.key.code == sf::Keyboard::Num4) {
					drawCitizens = !drawCitizens;
				}
				// press - to decrease simulation speed (up to minimum)
				if (event.key.code == sf::Keyboard::Subtract && !paused) {
					SIM_SPEED = std::min(std::max(SIM_SPEED - SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press + to increase simulation speed (up to maximum)
				if (event.key.code == sf::Keyboard::Add && !paused) {
					SIM_SPEED = std::min(std::max(SIM_SPEED + SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press p to toggle simulation pause
				// TODO pause thread instead of shutting sim speed (or, if lazy, just add a check if speed==0 and wait 1/FPS sec)
				if (event.key.code == sf::Keyboard::P) {
					paused = !paused;
					if (paused) {
						tempSimSpeed = SIM_SPEED;
						SIM_SPEED = 0;
					}
					else {
						SIM_SPEED = tempSimSpeed;
					}
				}
				// press space to spawn CUSTOM_CITIZEN_SPAWN_AMT citizens at the nearest node
				// TODO offload this to the pathfinding thread (should also prevent race conditions)
				if (event.key.code == sf::Keyboard::Space) {
					for (int i = 0; i < CUSTOM_CITIZEN_SPAWN_AMT; i++) {
						addCitizen(closestNode, &nodes[rand() % VALID_NODES]);
					}
				}
				// press semicolon to output citizen vector profile for debugging
				if (event.key.code == sf::Keyboard::SemiColon) {
					std::lock_guard<std::mutex> lock(citizensMutex);
					citizens.profile();
				}
			}
		}

		window.clear();

		window.setView(view);
		window.draw(bg);

		if (renderTick % TEXT_REFRESH_RATE == 0) {
			size_t c = citizens.activeSize();
			double s = simSpeedStat[simSpeedStat.size() - 1];
			text.setString(std::to_string(c) + " active citizens\n" + std::to_string(s) + " ticks/sec\n" + closestNode->id);
		}
		window.draw(text);

		// TODO FIXME for whatever godforsaken reason, this shit does not work with [] syntax and must use .append() and .clear()
		if (drawTrains) {
			std::lock_guard<std::mutex> trainLock(trainsMutex);
			trainVertices.clear();
			for (int i = 0; i < VALID_TRAINS; i++) {
				float newRadius = TRAIN_MIN_SIZE + trains[i].capacity / TRAIN_CAPACITY_FLOAT * (TRAIN_SIZE_DIFF);
				trains[i].updateRadius(newRadius);
				sf::Vector2f trainPositionNormalized = trains[i].getPosition() - sf::Vector2f(newRadius, newRadius);
				sf::Vector2f trainPosition = trains[i].getPosition();
				sf::Color trainColor = trains[i].getFillColor();
				for (int j = 0; j < TRAIN_N_POINTS; j++) {
					trainVertices.append(sf::Vertex(trains[i].getPoint(j) + trainPositionNormalized, trainColor));
					trainVertices.append(sf::Vertex(trainPosition, trainColor));
					trainVertices.append(sf::Vertex(trains[i].getPoint((j + 1) % TRAIN_N_POINTS) + trainPositionNormalized, trainColor));
				}
			}

			window.draw(trainVertices);
		}

		if (drawNodes) {
			nodeVertices.clear();
			for (int i = 0; i < VALID_NODES; i++) {

				//if (nodes[i].capacity > NODE_CAPACITY_WARN_THRESH) {
				//	 std::cout << "ERR unusually large node " << nodes[i].id << "(" << nodes[i].capacity << ")" << std::endl;
				//}

				float newRadius = NODE_MIN_SIZE + std::min(NODE_CAPACITY, nodes[i].capacity) / NODE_CAPACITY_FLOAT * (NODE_SIZE_DIFF);
				nodes[i].updateRadius(newRadius);
				sf::Vector2f nodePosition = nodes[i].getPosition();
				sf::Vector2f nodePositionNormalized = nodePosition - sf::Vector2f(newRadius, newRadius);
				sf::Color nodeColor = nodes[i].getFillColor();
				for (int j = 0; j < NODE_N_POINTS; j++) {
					nodeVertices.append(sf::Vertex(nodes[i].getPoint(j) + nodePositionNormalized, nodeColor));
					nodeVertices.append(sf::Vertex(nodePosition, nodeColor));
					nodeVertices.append(sf::Vertex(nodes[i].getPoint((j + 1) % NODE_N_POINTS) + nodePositionNormalized, nodeColor));
				}
			}

			window.draw(nodeVertices);
		}

		if (drawLines) {
			if (nodeCount == 2) {
				window.draw(userPathVertexBuffer);
			}
			else {
				window.draw(linesVertexBuffer);
			}
		}

		// TODO optimize citizen rendering
		if (drawCitizens) {
			std::lock_guard<std::mutex> citizenLock(citizensMutex);
			for (int i = 0; i < citizens.size(); i++) {
				if (citizens[i].status != STATUS_DESPAWNED) {
					window.draw(citizens[i]);
				}
			}
		}

		window.display();

		// frame rate control
		// vsync is for losers
		sf::Time frameTime = clock.getElapsedTime() - frameStart;
		sf::Int64 sleepTime = FRAME_DURATION.count() - frameTime.asMicroseconds();
		if (sleepTime > 0) {
			sf::sleep(sf::microseconds(sleepTime));
		}
	}
}

void pathfindingThread() {
	std::unique_lock<std::mutex> lock(pathsMutex);
	while (!shouldExit) {
		doPathfinding.wait(lock, [] {return !justDidPathfinding || shouldExit; });
		if (shouldExit) break;
		justDidPathfinding = true;
		int spawnAmount;
		#if CITIZEN_SPAWN_METHOD == 1
		spawnAmount = CITIZEN_SPAWN_MAX;
		if (CITIZEN_RANDOMIZE_SPAWN_AMT) {
			spawnAmount = rand() % spawnAmount;
		}
		#else
		spawnAmount = TARGET_CITIZEN_COUNT - (int)citizens.activeSize();
		#endif
		generateCitizens(spawnAmount);
	}
}

class CitizenThreadPool {
public:
	CitizenThreadPool(size_t numThreads) : stop(false) {
		for (size_t i = 0; i < numThreads; ++i) {
			workers.emplace_back([this] { workerThread(); });
		}
	}

	~CitizenThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		citizenThreadCV.notify_all();
		for (std::thread& worker : workers) {
			worker.join();
		}
	}

	template<class F>
	void enqueue(F&& f) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			tasks.emplace(std::forward<F>(f));
		}
		citizenThreadCV.notify_one();
	}

	void waitForCompletion() {
		std::unique_lock<std::mutex> lock(queueMutex);
		citizenThreadDoneCV.wait(lock, [this] { return tasks.empty() && activeThreads == 0; });
	}

private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queueMutex;
	std::condition_variable citizenThreadCV;
	std::condition_variable citizenThreadDoneCV;
	std::atomic<bool> stop;
	std::atomic<int> activeThreads{ 0 };

	void workerThread() {
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				citizenThreadCV.wait(lock, [this] { return stop || !tasks.empty(); });
				if (stop && tasks.empty()) {
					return;
				}
				task = std::move(tasks.front());
				tasks.pop();
			}
			activeThreads++;
			task();
			activeThreads--;
			if (tasks.empty() && activeThreads == 0) {
				citizenThreadDoneCV.notify_one();
			}
		}
	}
};

void debugStuckCitizens() {
	for (int i = 0; i < citizens.size(); i++) {
		Citizen& c = citizens[i];
		if (c.status != STATUS_DESPAWNED && c.timer > CITIZEN_DESPAWN_THRESH) {
			if (c.status == STATUS_AT_STOP) {
				c.getCurrentNode()->capacity = std::min(c.getCurrentNode()->capacity - 1, 0u);
				stuckMap[c.currentPathStr()]++;
			}
			c.status = STATUS_DESPAWNED_ERR;
		}
	}

	if (!stuckMap.empty()) {
		std::cout << "Citizens stuck at: " << std::endl;
		for (auto const& x : stuckMap) {
			std::cout << x.first << ": " << x.second << "\t";
		}
		std::cout << std::endl;
	}
	stuckMap.clear();
}

// TODO fix simulation hanging (must be mutex issue)

void simulationThread() {
	SIM_SPEED = DEFAULT_SIM_SPEED;
	double timeElapsed = double(clock());
	activeCitizensStat.reserve(BENCHMARK_RESERVE);
	clockStat.reserve(BENCHMARK_RESERVE);
	simSpeedStat.reserve(BENCHMARK_RESERVE);

	CitizenThreadPool pool(NUM_THREADS);

	while (!shouldExit) {

		simTick++;

		#if BENCHMARK_MODE == 1
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			std::cout << "\rProgress: " << float(simTick) / BENCHMARK_TICK_DURATION * 100 << "%" << ", " << citizens.activeSize() << " active citizens" << std::flush;
		}
		if (simTick >= BENCHMARK_TICK_DURATION) {
			std::cout << std::endl << "Benchmark concluded at tick " << simTick << std::endl;
			std::cout << "Handled " << handledCitizens << " citizens" << std::endl;
			shouldExit = true;
		}
		#endif

		// stats are recorded even outside of benchmark mode
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			activeCitizensStat.push_back(citizens.activeSize());
			clockStat.push_back(double(clock()));
			size_t clockSize = clockStat.size();
			if (clockSize > 1) {
				simSpeedStat.push_back(BENCHMARK_STAT_RATE / ((clockStat[clockSize-1] - clockStat[clockSize-2]) / CLOCKS_PER_SEC));
			}
			debugStuckCitizens();
		}
		
		if (simTick % CITIZEN_SPAWN_FREQ == 0) {
			std::lock_guard<std::mutex> lock(pathsMutex);
			justDidPathfinding = false;
			doPathfinding.notify_one();
		}

		{
			std::lock_guard<std::mutex> lock(trainsMutex);
			float trainSpeed = TRAIN_SPEED * SIM_SPEED;
			for (int i = 0; i < VALID_TRAINS; i++) {
				trains[i].updatePositionAlongLine(trainSpeed);
			}
		}

		{
			std::lock_guard<std::mutex> lock(citizensMutex);
			float citizenSpeed = CITIZEN_SPEED * SIM_SPEED;
			size_t chunkSize = citizens.activeSize() / NUM_THREADS + 1;

			for (int i = 0; i < NUM_THREADS; i++) {
				pool.enqueue([i, chunkSize, citizenSpeed]() {
					size_t start = i * chunkSize;
					size_t end = std::min(start + chunkSize, citizens.activeSize());
					for (size_t ind = start; ind < end; ++ind) {
						citizens.triggerCitizenUpdate(ind, citizenSpeed);
					}
					});
			}

			pool.waitForCompletion();
		}
	}
	std::cout << "Simulation time elapsed: " << (double(clock()) - timeElapsed) / CLOCKS_PER_SEC << "s" << std::endl;
	size_t averageActiveCitizens = 0; for (size_t i : activeCitizensStat) averageActiveCitizens += i;
	averageActiveCitizens /= activeCitizensStat.size();
	std::cout << "Averaged " << averageActiveCitizens << " citizen agents" << std::endl;

	doPathfinding.notify_one();
}

int main()
{
	// initialize simulation global arrays
	double progStartTime = double(clock());
	int initStatus = init();
	if (initStatus == AOK) {
		std::cout << "Simulation initialized successfully (" << (double(clock()) - progStartTime) / CLOCKS_PER_SEC << "s)" << std::endl;
	} else {
		return initStatus;
	}

	std::thread renThread;
	if (BENCHMARK_MODE) {
		std::cout << "Simulation running in benchmark mode (" << BENCHMARK_TICK_DURATION << " ticks)" << std::endl;
		std::cout << "Citizens spawn " << CITIZEN_SPAWN_MAX << "/" << CITIZEN_SPAWN_FREQ << " ticks, max " << MAX_CITIZENS << std::endl;
	}
	else {
		renThread = std::thread(renderingThread);
	}

	std::thread simThread(simulationThread);
	std::thread pathThread(pathfindingThread);

	if (!BENCHMARK_MODE && renThread.joinable()) {
		renThread.join();
	}
	if (simThread.joinable()) {
		simThread.join();
	}
	if (pathThread.joinable()) {
		pathThread.join();
	}

	std::cout << std::endl << "Done!" << std::endl;

	return 0;
}
