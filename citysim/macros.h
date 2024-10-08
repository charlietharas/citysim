#pragma once

// Graphics
#define WINDOW_WIDTH				1000
#define WINDOW_HEIGHT				1200
#define WINDOW_SCALE				0.5f
#define WINDOW_X_SCALE				2.0f
#define WINDOW_Y_SCALE				1.75f
#define WINDOW_X_OFFSET				100.0f
#define WINDOW_Y_OFFSET				-150.0f
#define ZOOM_AMT					0.1f
#define ZOOM_MAX					0.2f
#define ZOOM_MIN					1.5f
#define ARROW_PAN_AMT				6.0f
#define MAX_PAN_VELOCITY			15.0f // technically not velocity, measured in x/y direction
#define PAN_DECAY					0.9f // controls smoothness/gliding of arrow key pan
#define TARGET_FPS					60
#define ANTIALIAS_LEVEL				4
#define TEXT_FONT_SIZE				16
#define TRAIN_MIN_SIZE				5.0f
#define TRAIN_MAX_SIZE				10.0f
#define TRAIN_SIZE_DIFF				TRAIN_MAX_SIZE - TRAIN_MIN_SIZE
#define TRAIN_N_POINTS				20
#define NODE_MIN_SIZE				2.5f
#define NODE_MAX_SIZE				15.0f
#define NODE_SIZE_DIFF				NODE_MAX_SIZE - NODE_MIN_SIZE
#define NODE_N_POINTS				8
#define TEXT_REFRESH_RATE			10 // every n frames
#define BACKGROUND_COLOR			sf::Color::White

// Simulation size
#define MAX_LINES					32
#define MAX_NODES					512
#define MAX_TRAINS					1024
#define MAX_CITIZENS				200000
#define NUM_CITIZEN_WORKER_THREADS	8 // important to adjust for performance depending on your machine
#define DISTANCE_SCALE				128

// File loading
#define STATIONS_CSV_NUM_COLUMNS	6
#define GEOM_CSV_NUM_COLUMNS		9

// Node and Train status flags
#define STATUS_DESPAWNED			0
#define STATUS_SPAWNED				1
#define STATUS_IN_TRANSIT			2
#define STATUS_AT_STOP				3
#define STATUS_TRANSFER				4
#define STATUS_WALK					5
#define STATUS_BOARDED				6
#define STATUS_FORWARD				1
#define STATUS_BACKWARD				-1
#define STATUS_AMBIVALENT			3
#define STATUS_HIGHLIGHTED			2

// Line
#define LINE_PATH_SIZE				64
#define LINE_ID_SIZE				4 // size of char buffer
#define WALK_LINE_ID_STR			"WLK"

// Nodes
#define NODE_ID_SIZE				36 // size of char buffer
#define NODE_CAPACITY				256u // cosmetic only
#define NODE_GRID_ROWS				10
#define NODE_GRID_COLS				12

// Train
#define DEFAULT_TRAIN_STOP_SPACING	4 // spawn trains every n stops
#define TRAIN_SPEED					8.0f
#define TRAIN_CAPACITY				750 // citizens cannot board trains if they are at max capacity
#define TRAIN_STOP_THRESH			750 * TRAIN_SPEED // how long trains wait at stops

// Citizen
constexpr float CITIZEN_SPEED = 1.0f;
#define	CITIZEN_TRANSFER_THRESH		64 * CITIZEN_SPEED // how long citizens walk through stations before waiting for a train
#define CITIZEN_SPAWN_INIT			4000 // initial amount of citizens to spawn before simulation start
#define CITIZEN_SPAWN_FREQ			1024 // spawn citizens every n simulation ticks
#define CITIZEN_CULL_FREQ			4096 // cull citizens who have been stuck longer than CITIZEN_DESPAWN_THRESH every n simulation ticks
#define CITIZEN_SPAWN_METHOD		0 // 0 to match target amount, 1 for fixed amount (CITIZEN_SPAWN_AMT)
#define CITIZEN_SPAWN_AMT			2000
#define TARGET_CITIZEN_COUNT		40000
#define CITIIZEN_VEC_RESERVE		TARGET_CITIZEN_COUNT * 2
#define CUSTOM_CITIZEN_SPAWN_AMT	250
#define CITIZEN_DESPAWN_THRESH		CITIZEN_DESPAWN_WARN * 8

// Pathfinding
#define CITIZEN_PATH_SIZE			64 // this value is not mathematically guaranteed to exceed the maximum number of possible lines in a path (but errors are handled)
#define NODE_N_NEIGHBORS			16
#define NODE_N_TRAINS				8
#define TRANSFER_MAX_DIST			10.0f
#define STOP_PENALTY				20 // fixed penalty for each stop
#define TRANSFER_PENALTY			STOP_PENALTY * 2 // fixed penalty for transferring to another line/walking
#define TRANSFER_PENALTY_MULTIPLIER TRAIN_SPEED / CITIZEN_SPEED // multiplier for distance walked during walking transfers

// PathCache
constexpr int PATH_CACHE_BUCKETS = 200;
constexpr int PATH_CACHE_BUCKETS_SIZE = 32;
constexpr int CACHE_TRANSFERS_THRESHOLD = 2; // paths with n or different lines are cached
#define PRIME_1 541
#define PRIME_2 1223

// Debugging
#define AOK							0
#define ERROR_OPENING_FILE			1
#define BENCHMARK_MODE				false
#define BENCHMARK_TICK_AMT			50000
#define STAT_RATE					1000 // every n simulation ticks
#define BENCHMARK_RESERVE			BENCHMARK_TICK_AMT / STAT_RATE * 2
#define USER_INFO_MODE				true
#define PATHFINDER_ERRORS			false
#define TRAIN_ERRORS				false
#define CITIZEN_SPAWN_ERRORS		false
#define DISABLE_SIMULATION			false
#define NODE_CAPACITY_WARN			512
#define CITIZEN_DESPAWN_WARN		500000 * CITIZEN_SPEED
#define CITIZEN_STUCK_THRESH		10 // ignore nodes with below n stuck citizens when outputting debug info