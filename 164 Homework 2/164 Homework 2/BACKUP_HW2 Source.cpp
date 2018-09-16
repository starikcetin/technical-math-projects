/*

	 CTIS 164
	----------

	S. Tarýk Çetin
	21702765
	Section 1
	Homework #2

	----------

	Features which I took the liberty of implementing:

	+ Rocket exhausts. Uses a particle system that spawns circles.
		+ Particles' green color component increases over time while red stays same, this creates a from red to yellow color change.
		+ Particles' size decrease over time, this creates a rocket flame effect.

	+ Bullet trail. Uses a particle system that spawns circles.
		+ Particles' alpha color component decrease over time, this creates a fading trail effect.
		+ Particles' size decrease over time slightly, just like with real AA shells.

	+ Gradient sky background.

	+ Targets (rockets) randomize their Y coordinate on each spawn.

	+ Frame rate independent movement: My update methods use deltaTime which I measure independently from the frame rate. This provides smooth and deterministic movement.

	+ Console reports: 
		+ Collision detector reports details of hits to console.
		+ Session controller reports end of sessions to console.

	+ Almost all game parameters are in define statements. I tried to minimize hard-coding.

*/





//
// Dependencies
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string.h>
#include <math.h>
#include <time.h>
#include <random>

#include <GL/glut.h>





//
// Constants
//

#define DEG_2_RAD 0.0174532

// The time passed in milliseconds since the beginning of the application
#define NOW glutGet(GLUT_ELAPSED_TIME)

//
// system parameters

#define TIMER_PERIOD_GAME_UPDATE 1

#define MAX_PARTICLES 100
#define PARTICLE_LIFESPAN 500
#define PARTICLE_CREATE_COOLDOWN 10

//
// strings

#define STRING_STATE_SHORT_NOT_STARTED "Welcome"
#define STRING_STATE_SHORT_RUNNING "Running"
#define STRING_STATE_SHORT_PAUSED "Paused"
#define STRING_STATE_SHORT_FINISHED "Finished"

#define STRING_STATE_DETAILS_NOT_STARTED "Press F1 to initiate the first session."
#define STRING_STATE_DETAILS_RUNNING "Press Spacebar to fire. Press F1 to pause."
#define STRING_STATE_DETAILS_PAUSED "Press F1 to resume."
#define STRING_STATE_DETAILS_FINISHED "Press F1 to initiate a new session."

//
// layout parameters

#define WIDTH_FIXED			1000
#define HEIGHT_SCOREBOARD	100
#define HEIGHT_KEYS_PANEL	30
#define HEIGHT_GAME_AREA	500
#define HEIGHT_GRASS		50

#define TOTAL_WIDTH			WIDTH_FIXED
#define TOTAL_HEIGHT		(HEIGHT_SCOREBOARD + HEIGHT_KEYS_PANEL + HEIGHT_GAME_AREA + HEIGHT_GRASS)

//window
#define WINDOW_LEFT		-TOTAL_WIDTH / 2
#define WINDOW_RIGHT	-1 * WINDOW_LEFT

//scoreboard
#define SB_TOP			TOTAL_HEIGHT / 2
#define SB_BOT			SB_TOP - HEIGHT_SCOREBOARD

//key panel
#define KP_TOP			SB_BOT
#define KP_BOT			KP_TOP - HEIGHT_KEYS_PANEL
#define KP_MID			(KP_TOP + KP_BOT) / 2

//game area
#define GA_TOP			KP_BOT
#define GA_BOT			GA_TOP - HEIGHT_GAME_AREA
#define GA_MID			(GA_TOP + GA_BOT) / 2

//grass
#define GR_TOP			GA_BOT
#define GR_BOT			GR_TOP - HEIGHT_GRASS

//
// sizes and dimensions
//		Note: These are for the collision checkers.
//			  Altering these won't change the appearence of objects.
//			  To change appearence, take a look at the drawer methods.

//rockets
#define ROCKET_WIDTH	160
#define ROCKET_HEIGHT	50

#define TANK_WIDTH		100

//bullet
#define BULLET_WIDTH	10
#define BULLET_HEIGHT	40

// hitboxes
#define HITBOX_ROCKET_HEIGHT 20

//
// game parameters

#define MAX_POINTS_PER_HIT	5
#define MIN_POINTS_PER_HIT	1
#define SESSION_LENGTH_SEC	20
#define SPEED_TANK			0.25
#define SPEED_BULLET		0.25
#define SPEED_ROCKET		0.25
#define ROCKET_COUNT		5





//
// Custom Type Definitions
//

typedef enum { notStarted, running, paused, finished } gameState_e;

typedef struct
{
	double x;
	double y;
} vector2_t;

typedef struct
{
	vector2_t globalPos;
	double aliveTime; // time spent alive
	bool isAlive;
} particle_t;

typedef struct
{
	// all particles
	particle_t particlePool[MAX_PARTICLES];

	// the index of the last created particle
	int lastUsedParticleIndex;

	// the last time a particle was created
	double lastParticleCreationTime;
} particleSystem_t;

typedef struct
{
	vector2_t pivotPos;
	bool alive;
	bool counted;

	particleSystem_t exhaustPS;

} rocket_t;





//
// Global Variables
//

int m_windowWidth, m_windowHeight;

int m_lastTimerTickTime;
gameState_e m_currentGameState = notStarted;

bool m_input_left;
bool m_input_right;

vector2_t m_pos_tankPivot;		// position of the pivot of the tank
vector2_t m_pos_bulletCenter;	// position of the pivot of the tank
rocket_t m_rockets[ROCKET_COUNT];

bool m_bulletIsInTheAir;
particleSystem_t m_bulletTrailPS;

double m_sessionTotalScore;
double m_lastHitScore;
double m_sessionTime;
int m_sessionRocketsCreated;
int m_sessionRocketsHit;




//
// Utility Functions
//

// converts 'value' from (aMin-aMax) to (bMin-bMax) range, keeps the ratio.
double rangeConversion(const double value, const double aMin, const double aMax, const double bMin, const double bMax)
{
	return (((value - aMin) * (bMax - bMin)) / (aMax - aMin)) + bMin;
}

// draws a circle with the center (x, y) and radius r
void drawCircle(const int x, const int y, const int r)
{
#define PI 3.1415
	float angle;
	glBegin(GL_POLYGON);
	for (int i = 0; i < 100; i++)
	{
		angle = 2 * PI*i / 100;
		glVertex2f(x + r*cos(angle), y + r*sin(angle));
	}
	glEnd();
}

// Draws a rectangle with Left, Top, Width, Height properties.
void drawRectd_LTWH(const double left, const double top, const double width, const double height)
{
	glRectd(left, top,
		left + width, top - height);
}

// Draws a triangle which faces right.
// center: Center coordinates.
// width: Total width of the shape.
// height: Total height of the shape.
// lookDirection: If positive, tirangle looks to positive; if negative, triangle looks to negative. Magnitudes greater than 1 will stretch the triangle.
void drawTriangle_horizontal(const vector2_t *center, const double width, const double height, const int lookDirection)
{
	glBegin(GL_POLYGON);
	glVertex2d(center->x - (width * lookDirection / 2), center->y + height / 2); //top
	glVertex2d(center->x + (width * lookDirection / 2), center->y); //head
	glVertex2d(center->x - (width * lookDirection / 2), center->y - height / 2); //bottom
	glEnd();
}

// Converts a Y coordinate from header local coordinates to global coordinates.
double localToGlobal_Y(const double y, const double headerHeight)
{
	// shift up half window, then shift down half header height
	return y + (m_windowHeight / 2) - (headerHeight / 2);
}

// Prints a string on (x, y) coordinates.
// lean: -1 = left-lean   0 = centered   1 = right-lean
void drawBitmapString(const int lean, const int x, const int y, void *font, const char *string)
{
	int len = (int)strlen(string);

	double leanShift = (lean + 1) * glutBitmapLength(font, (unsigned char*)string) / 2.0;
	glRasterPos2f(x - leanShift, y);

	for (int i = 0; i < len; i++)
	{
		glutBitmapCharacter(font, string[i]);
	}
}

// Prints a string on (x, y) as left coordinates, allows variables in string.
// drawString_WithVars(0, -winWidth / 2 + 10, winHeight / 2 - 20, GLUT_BITMAP_8_BY_13, "ERROR: %d", numClicks);
// lean: -1 = left-lean   0 = centered   1 = right-lean
void drawBitmapString_WithVars(const int lean, const int x, const int y, void *font, const char *string, ...)
{
	va_list ap;
	va_start(ap, string);
	char str[1024];
	vsprintf_s(str, string, ap);
	va_end(ap);

	int len = (int)strlen(str);

	double leanShift = (lean + 1) * glutBitmapLength(font, (unsigned char*)str) / 2.0;
	glRasterPos2f(x - leanShift, y);

	for (int i = 0; i < len; i++)
	{
		glutBitmapCharacter(font, str[i]);
	}
}

// Prints a string on (x, y) as left coordinates, allows variables in string.
// drawStrokeString_WithVars(0, -50, 0, 0.35, "00:%02d", timeCounter);
// lean: -1 = right-lean   0 = centered   1 = left-lean
void drawStrokeString_WithVars(const int lean, const int x, const int y, void *font, const float size, const char *string, ...) {
	va_list ap;
	va_start(ap, string);
	char str[1024];
	vsprintf_s(str, string, ap);
	va_end(ap);

	double leanShift = size * (lean + 1) * glutStrokeLength(font, (unsigned char*)str) / 2.0;

	glPushMatrix();
	glTranslatef(x - leanShift, y, 0);
	glScalef(size, size, 1);

	int len, i;
	len = (int)strlen(str);
	for (i = 0; i<len; i++)
	{
		glutStrokeCharacter(font, str[i]);
	}
	glPopMatrix();
}

// returns a random double in a range of (min-max)
// no safety checks, be vary of overflows
double randomDouble(const double min, const double max)
{
	return rangeConversion(rand(), 0, RAND_MAX, min, max);
}

// returns the string constant explaining the current state
char* getCurrentStateText_State()
{
	switch (m_currentGameState)
	{
	case notStarted: return STRING_STATE_SHORT_NOT_STARTED;
	case running: return STRING_STATE_SHORT_RUNNING;
	case paused: return STRING_STATE_SHORT_PAUSED;
	case finished: return STRING_STATE_SHORT_FINISHED;
	}

	return "---- error_string ----";
}

// returns the string constant explaining the current state's details
char* getCurrentStateText_Details()
{
	switch (m_currentGameState)
	{
	case notStarted: return STRING_STATE_DETAILS_NOT_STARTED;
	case running: return STRING_STATE_DETAILS_RUNNING;
	case paused: return STRING_STATE_DETAILS_PAUSED;
	case finished: return STRING_STATE_DETAILS_FINISHED;
	}

	return "---- error_string ----";
}





//
// Particle System Methods
//

// returns a particle.
// ps: particle system
// isForUse: if true, 'lastUsedParticleIndex' will be incremented by 1.
particle_t* getAvailableExhaustParticle(particleSystem_t *ps, const bool isForUse)
{
	int toReturn = ps->lastUsedParticleIndex;

	if (isForUse)
	{
		ps->lastUsedParticleIndex = (ps->lastUsedParticleIndex + 1) % MAX_PARTICLES;
	}

	return &(ps->particlePool[toReturn]);
}

// calculates and kills overdue particles
// ps: particle system
void killOverdueParticles(particleSystem_t *ps)
{
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		if (ps->particlePool[i].isAlive && ps->particlePool[i].aliveTime >= PARTICLE_LIFESPAN)
		{
			// kill
			ps->particlePool[i].isAlive = false;
			ps->particlePool[i].aliveTime = 0;
		}
	}
}

// kills all particles in this particle system
// ps: particle system
void killAllParticles(particleSystem_t *ps)
{
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		// kill
		ps->particlePool[i].isAlive = false;
		ps->particlePool[i].aliveTime = 0;
	}
}

// uses the oldest particle to make a new exhaust particle; then returns the particle
// ps: particle system
// position: the spawn posiiton for the particle
particle_t* popExhaustParticle(particleSystem_t *ps, const vector2_t *position)
{
	particle_t *particle = getAvailableExhaustParticle(ps, true);

	particle->aliveTime = 0;

	particle->globalPos.x = position->x;
	particle->globalPos.y = position->y;

	particle->isAlive = true;

	return particle;
}

// resets this particle system
// ps: particle system
void resetParticleSystem(particleSystem_t *ps)
{
	ps->lastUsedParticleIndex = 0;
	ps->lastParticleCreationTime = 0;
	killAllParticles(ps); // force kill all particles
}





//
// Game Logic
//


double randomRocketY()
{
	// top half of the game area
	return randomDouble(GA_MID + (ROCKET_HEIGHT / 2), GA_TOP - (ROCKET_HEIGHT / 2));
}

// moves rocket to left edge of screen
// r : the rocket
void moveRocketToLeft(rocket_t *rocket)
{
	rocket->pivotPos.x = WINDOW_LEFT - ROCKET_WIDTH;
	rocket->pivotPos.y = randomRocketY();
}

// if a rocket fully passes the right edge of screen:
// 1. moves it to the left edge
// 2. resurrects it
//
// if a rocket hits (slightly passes) the left edge of the screen:
// increments the rocket count
void rocketEdgeDetectAndHandle(rocket_t *rocket)
{
	if (rocket->pivotPos.x + ROCKET_WIDTH > WINDOW_LEFT && !rocket->counted)
	{
		rocket->counted = true;
		m_sessionRocketsCreated++;
	}
	else if (rocket->pivotPos.x >= WINDOW_RIGHT)
	{
		moveRocketToLeft(rocket);
		rocket->alive = true;
		rocket->counted = false;
	}
}

// moves bullet back to barrel and sets m_bulletIsInTheAir flag to false.
void resetBulletToBarrel()
{
	//relative barrel positions for bullet from the pivot of tank
	double relBarrel_Y = 70;
	double relBarrel_X = 5;

	m_pos_bulletCenter.x = m_pos_tankPivot.x + relBarrel_X;
	m_pos_bulletCenter.y = m_pos_tankPivot.y + relBarrel_Y;

	m_bulletIsInTheAir = false;
}

void fireBullet()
{
	m_bulletIsInTheAir = true;
}

//void handleCollision(int rocketIndex)
//{
//
//
//}

void detectAndResolveCollisions()
{
	vector2_t bulletCenter = m_pos_bulletCenter;

	double b_top = bulletCenter.y + BULLET_HEIGHT / 2;
	double b_bot = bulletCenter.y - BULLET_HEIGHT / 2;
	double b_left = bulletCenter.x - BULLET_WIDTH / 2;
	double b_right = bulletCenter.x + BULLET_WIDTH / 2;
	double b_mid = (b_left + b_right) / 2;
	double halfBulletWidth = BULLET_WIDTH / 2;

	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		if (!m_rockets[i].alive) continue;

		vector2_t rocketPivot = m_rockets[i].pivotPos;

		double r_top = rocketPivot.y + HITBOX_ROCKET_HEIGHT / 2;
		double r_bot = rocketPivot.y - HITBOX_ROCKET_HEIGHT / 2;
		double r_left = rocketPivot.x;
		double r_right = rocketPivot.x + ROCKET_WIDTH;
		double halfRocketWidth = ROCKET_WIDTH / 2;

		// RETURN IF

		// 1. bullet left is on the right of rocket right
		if (b_left > r_right) continue;

		// 2. bullet right is on the left of rocket left
		else if (b_right < r_left) continue;

		// 3. bullet bottom is on the top of rocket top
		else if (b_bot > r_top) continue;

		// 4. bullet top is on the bottom of rocket bottom
		else if (b_top < r_bot) continue;

		// ELSE we got a collision.
		else
		{
			m_rockets[i].alive = false;

			// calculate how centered the hit was
			double r_mid = (r_left + r_right) / 2;
			double distanceFromCenter = abs(b_mid - r_mid);

			double points = rangeConversion(distanceFromCenter, 0, halfRocketWidth + halfBulletWidth, MAX_POINTS_PER_HIT, MIN_POINTS_PER_HIT);

			// update scores
			m_lastHitScore = points;
			m_sessionTotalScore += points;
			m_sessionRocketsHit++;

			// DEBUG
			printf(" ****** \n collison \n"); // DEBUG MESSAGE
			printf(" r_mid : %0.2f \n b_mid : %0.2f \n", r_mid, b_mid); // DEBUG MESSAGE
			printf(" distanceFromCenter : %0.2f \n halfTotaltWidth : %0.2f \n", distanceFromCenter, halfRocketWidth + halfBulletWidth); // DEBUG MESSAGE
			printf(" points : %0.2f \n ****** \n\n", points); // DEBUG MESSAGE
		}
	}
}

void bulletEdgeDetectAndReset()
{
	if (m_pos_bulletCenter.y - (BULLET_HEIGHT / 2) > KP_BOT)
	{
		resetBulletToBarrel();
	}
}

void updateExhaust(rocket_t *rocket, const double deltaTime)
{
	// cleanup
	killOverdueParticles(&(rocket->exhaustPS));

	// create a new particle if it is the time and the rocket is alive
	if (rocket->alive && NOW - rocket->exhaustPS.lastParticleCreationTime > PARTICLE_CREATE_COOLDOWN)
	{
		// reset the timer
		rocket->exhaustPS.lastParticleCreationTime = NOW;

		// create the particle
		popExhaustParticle(&(rocket->exhaustPS), &(rocket->pivotPos));
	}

	//update all alive particles' aliveTime
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		if (rocket->exhaustPS.particlePool[i].isAlive)
		{
			rocket->exhaustPS.particlePool[i].aliveTime += deltaTime;
		}
	}
}

void updateBulletTrail(const double deltaTime)
{
	// cleanup
	killOverdueParticles(&m_bulletTrailPS);

	// create a new particle if it is the time and the bullet is in the air
	if (m_bulletIsInTheAir && NOW - m_bulletTrailPS.lastParticleCreationTime > PARTICLE_CREATE_COOLDOWN)
	{
		// reset the timer
		m_bulletTrailPS.lastParticleCreationTime = NOW;

		// create the particle
		popExhaustParticle(&m_bulletTrailPS, &m_pos_bulletCenter);
	}

	//update all alive particles' aliveTime
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		if (m_bulletTrailPS.particlePool[i].isAlive)
		{
			m_bulletTrailPS.particlePool[i].aliveTime += deltaTime;
		}
	}
}

// responsible for game update.
void updateGame(const double deltaTime)
{
	// deltatime is in milliseconds.

	// tank
	if (m_input_left && m_pos_tankPivot.x - TANK_WIDTH / 2 > WINDOW_LEFT)
	{
		m_pos_tankPivot.x -= SPEED_TANK * deltaTime;
	}
	else if (m_input_right && m_pos_tankPivot.x + TANK_WIDTH / 2 < WINDOW_RIGHT)
	{
		m_pos_tankPivot.x += SPEED_TANK * deltaTime;
	}


	// rockets
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		m_rockets[i].pivotPos.x += SPEED_ROCKET * deltaTime;
		rocketEdgeDetectAndHandle(&m_rockets[i]);
		updateExhaust(&m_rockets[i], deltaTime);
	}


	// bullet
	if (m_bulletIsInTheAir)
	{
		m_pos_bulletCenter.y += SPEED_BULLET * deltaTime;

		detectAndResolveCollisions();
		bulletEdgeDetectAndReset();
	}
	else
	{
		resetBulletToBarrel();
	}

	updateBulletTrail(deltaTime);


	// session
	m_sessionTime += deltaTime;

	if (m_sessionTime >= SESSION_LENGTH_SEC * 1000)
	{
		m_sessionTime = SESSION_LENGTH_SEC * 1000;
		m_currentGameState = finished;
		printf(" ### session finished ### \n\n"); // DEBUG PRINT
	}
}





//
// Initialization
//

void initializeData()
{
	m_lastHitScore = 0;
	m_sessionTotalScore = 0;
	m_sessionTime = 0;
	m_sessionRocketsCreated = 0;
	m_sessionRocketsHit = 0;
}

void initializeGame()
{
	//reset tank
	m_pos_tankPivot.x = 0;
	m_pos_tankPivot.y = GR_TOP;

	//reset bullet
	resetBulletToBarrel();
	resetParticleSystem(&m_bulletTrailPS);

	//reset rockets
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		// random X at the invisible space towards the left, at a range of screen width
		//double randomX = randomDouble(WINDOW_LEFT - TOTAL_WIDTH - ROCKET_WIDTH, WINDOW_LEFT - ROCKET_WIDTH);
		double randomX = rangeConversion(i, 0, ROCKET_COUNT - 1, WINDOW_LEFT - TOTAL_WIDTH - ROCKET_WIDTH, WINDOW_LEFT - ROCKET_WIDTH);

		m_rockets[i].pivotPos.x = randomX;
		m_rockets[i].pivotPos.y = randomRocketY();

		m_rockets[i].alive = true;
		m_rockets[i].counted = false;

		resetParticleSystem(&(m_rockets[i].exhaustPS));
	}
}

// Initailizes Game Objects and Data, in that order.
void initializeNewSession()
{
	srand(time(0));

	initializeGame();
	initializeData();
}




//
// Drawers
//

// Draws backgrounds
void drawBackgrounds_ExceptGameArea()
{
	// scoreboard rgb(25, 42, 86)
	glColor3ub(25, 42, 86);
	glRectd(WINDOW_LEFT, SB_TOP, WINDOW_RIGHT, SB_BOT);


	// keys panel rgb(239, 228, 166)
	glColor3ub(239, 228, 166);
	glRectd(WINDOW_LEFT, KP_TOP, WINDOW_RIGHT, KP_BOT);


	// grass rgb(68, 189, 50)
	glColor3ub(78, 189, 60);
	glRectd(WINDOW_LEFT, GR_TOP, WINDOW_RIGHT, GR_BOT);
}

// Draw game area background
void drawGameAreaBackground()
{
	// game area rgb(0, 151, 230)
	//glColor3ub(0, 151, 230);
	//glRectd(WINDOW_LEFT, GA_TOP, WINDOW_RIGHT, GA_BOT);


	// Top - Middle
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_POLYGON);

	glColor3d(0, 0.5, 1); // dark sky blue
	glVertex2d(WINDOW_LEFT, GA_TOP); // top-left

	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(WINDOW_LEFT, GA_MID); // middle-left

	//glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(WINDOW_RIGHT, GA_MID); // middle-right

	glColor3d(0, 0.5, 1); // dark sky blue
	glVertex2d(WINDOW_RIGHT, GA_TOP); // top-right
	glEnd();

	// Middle - Bottom
	glBegin(GL_POLYGON);
	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(WINDOW_LEFT, GA_MID); // middle-left

	glColor3d(0.3, 0.8, 1); // sky blue
	glVertex2d(WINDOW_LEFT, GA_BOT); // bottom-left

	//glColor3d(0.3, 0.8, 1); // sky blue
	glVertex2d(WINDOW_RIGHT, GA_BOT); // bottom-right

	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(WINDOW_RIGHT, GA_MID); // middle-right
	glEnd();
}

// pivot of tank is it's bottom, 
// when facing upright it will be te downmost
void drawTank(vector2_t pivot)
{
	//body
	glColor3ub(85, 72, 64);
	

	glBegin(GL_POLYGON);
	glVertex2d(-50	+ pivot.x,	10 + pivot.y);
	glVertex2d(-50	+ pivot.x,	30 + pivot.y);
	glVertex2d(-35	+ pivot.x,	40 + pivot.y);
	glVertex2d(35	+ pivot.x,	40 + pivot.y);
	glVertex2d(50	+ pivot.x,	30 + pivot.y);
	glVertex2d(50	+ pivot.x,	10 + pivot.y);
	glEnd();

	//gun
	
	glColor3ub(158, 154, 117);

	glRectd(-20	+ pivot.x,	50	+ pivot.y,
			-5	+ pivot.x,	40	+ pivot.y);

	glColor3ub(65, 83, 59);

	glRectd(-5	+ pivot.x,	100	+ pivot.y,
			15	+ pivot.x,	40	+ pivot.y);

	//wheels
	glColor3ub(28, 34, 46);

	drawCircle(-35 + pivot.x, 5 + pivot.y, 10);
	drawCircle(0 + pivot.x, 5 + pivot.y, 10);
	drawCircle(35 + pivot.x, 5 + pivot.y, 10);

	glColor3ub(65, 153, 59);

	drawCircle(-35 + pivot.x, 5 + pivot.y, 2);
	drawCircle(0 + pivot.x, 5 + pivot.y, 2);
	drawCircle(35 + pivot.x, 5 + pivot.y, 2);
}

// pivot of rocket is it's bottom, 
// when facing right it will be the leftmost
void drawRocket(vector2_t pivot)
{
	//tail
	glColor3d(0.15, 0.15, 0.2); // dusty black

	glBegin(GL_POLYGON);
	glVertex2d(0 + pivot.x, 25 + pivot.y);
	glVertex2d(25 + pivot.x, 25 + pivot.y);
	glVertex2d(50 + pivot.x, 0 + pivot.y);
	glVertex2d(25 + pivot.x, -25 + pivot.y);
	glVertex2d(0 + pivot.x, -25 + pivot.y);
	glEnd();

	//body
	glColor3d(0.9, 0.85, 0.9); // dusty white

	glRectd(0 + pivot.x, 10 + pivot.y,
		135 + pivot.x, -10 + pivot.y);

	//pilot cabin
	glColor3d(0.1, 0.3, 0.5); // glass blue + black

	glBegin(GL_POLYGON);
	glVertex2d(65 + pivot.x, 10 + pivot.y);
	glVertex2d(110 + pivot.x, 14 + pivot.y);
	glVertex2d(120 + pivot.x, 14 + pivot.y);
	glVertex2d(130 + pivot.x, 10 + pivot.y);
	glEnd();

	//nose cone
	glColor3d(0.65, 0, 0); // maroon

	glBegin(GL_POLYGON);
	glVertex2d(135 + pivot.x, 10 + pivot.y); //top
	glVertex2d(160 + pivot.x, 0 + pivot.y); //head
	glVertex2d(135 + pivot.x, -10 + pivot.y); //bottom
	glEnd();
}

void drawBullet(vector2_t center)
{
	glColor3ub(152, 136, 100); // black

	//body
	glRectd(-5	+ center.x,	10	+ center.y,
			5	+ center.x,	-20	+ center.y);

	//nose cone
	glBegin(GL_POLYGON);
	glVertex2d(-5 + center.x, 10 + center.y);
	glVertex2d(0 + center.x, 20 + center.y); //head
	glVertex2d(5 + center.x, 10 + center.y);
	glEnd();
}

void drawGameObjects()
{
	drawTank(m_pos_tankPivot);
	drawBullet(m_pos_bulletCenter);

	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		if (!m_rockets[i].alive)
		{
			continue;
		}

		drawRocket(m_rockets[i].pivotPos);
	}
}

void drawScoreboardTexts()
{
	//	remaining time
	glColor3ub(255, 255, 255);
	drawBitmapString			(0, WINDOW_LEFT + 100, SB_TOP - 40, GLUT_BITMAP_8_BY_13, "REMAINING TIME");
	drawBitmapString_WithVars	(0, WINDOW_LEFT + 100, SB_BOT + 40, GLUT_BITMAP_8_BY_13, "%0.3f", SESSION_LENGTH_SEC - m_sessionTime / 1000);

	//	total number of rockets created
	//glColor3ub(255, 255, 255);
	drawBitmapString			(0, WINDOW_LEFT + 300, SB_TOP - 40, GLUT_BITMAP_8_BY_13, "ROCKETS SO FAR");
	drawBitmapString_WithVars	(0, WINDOW_LEFT + 300, SB_BOT + 40, GLUT_BITMAP_8_BY_13, "%d", m_sessionRocketsCreated);
	

	//	number of rockets hit
	//glColor3ub(255, 255, 255);
	drawBitmapString		 (0, WINDOW_LEFT + 500, SB_TOP - 40, GLUT_BITMAP_8_BY_13, "NEUTRALIZED TARGETS");
	drawBitmapString_WithVars(0, WINDOW_LEFT + 500, SB_BOT + 40, GLUT_BITMAP_8_BY_13, "%d", m_sessionRocketsHit);


	//	points gained with the last hit
	//glColor3ub(255, 255, 255);
	drawBitmapString		 (0, WINDOW_LEFT + 700, SB_TOP - 40, GLUT_BITMAP_8_BY_13, "LAST HIT SCORE");
	drawBitmapString_WithVars(0, WINDOW_LEFT + 700, SB_BOT + 40, GLUT_BITMAP_8_BY_13, "%0.3f", m_lastHitScore);


	//	total points(score)
	//glColor3ub(255, 255, 255);
	drawBitmapString		 (0, WINDOW_LEFT + 900, SB_TOP - 40, GLUT_BITMAP_8_BY_13, "TOTAL SCORE");
	drawBitmapString_WithVars(0, WINDOW_LEFT + 900, SB_BOT + 40, GLUT_BITMAP_8_BY_13, "%0.3f", m_sessionTotalScore);
}

void drawKeyPanelTexts()
{
	glColor3ub(0, 0, 0);
	drawBitmapString(0, 0, KP_MID - 5, GLUT_BITMAP_8_BY_13, getCurrentStateText_Details());
}

void drawTexts()
{
	// scoreboard
	drawScoreboardTexts();

	// key panel
	drawKeyPanelTexts();
}

void drawExhaust(const rocket_t *rocket)
{
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		particle_t particle = rocket->exhaustPS.particlePool[i];

		if (particle.isAlive)
		{
			double greenFromTime = rangeConversion(particle.aliveTime, 0, PARTICLE_LIFESPAN, 0, 1);
			double sizeFromTime = rangeConversion(particle.aliveTime, 0, PARTICLE_LIFESPAN, 10, 1);

			glColor3d(1, greenFromTime, 0); // red -> yellow
			drawCircle(particle.globalPos.x, particle.globalPos.y, sizeFromTime);
		}
	}
}

void drawBulletTrail()
{
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		particle_t particle = m_bulletTrailPS.particlePool[i];

		if (particle.isAlive)
		{
			double alphaFromTime = rangeConversion(particle.aliveTime, 0, PARTICLE_LIFESPAN, 1, 0);
			double sizeFromTime = rangeConversion(particle.aliveTime, 0, PARTICLE_LIFESPAN, 5, 3);

			glColor4d(1, 0.75, 0, alphaFromTime);
			drawCircle(particle.globalPos.x, particle.globalPos.y, sizeFromTime);
		}
	}
}

void drawParticles()
{
	// rocket exhausts
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		drawExhaust(&m_rockets[i]);
	}

	// bullet trail
	drawBulletTrail();
}

// Draws Statics, Game Objects, and Texts; in that order.
void draw()
{
	drawGameAreaBackground(); // game area background is drawn before so game objects are visible.
	drawParticles();
	drawGameObjects();
	drawBackgrounds_ExceptGameArea(); // all other backgrounds are drawn after so they are over game objects.
	drawTexts();
}





//
// GLUT Event Handlers
//

// F1 key press handler.
void onF1Pressed()
{
	switch (m_currentGameState)
	{
	case notStarted:
		// already initialized the game in main()
		m_currentGameState = running;
		break;

	case running:
		m_currentGameState = paused;
		break;

	case paused:
		m_currentGameState = running;
		break;

	case finished:
		initializeNewSession();
		m_currentGameState = running;
		break;

	}
}

// Speacebar key press handler.
void onSpacebarPressed()
{
	if (m_currentGameState == running)
	{
		fireBullet();
	}
}

// To display onto window using OpenGL commands
void display()
{
	// Clear to black
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	// Draw game
	draw();

	glutSwapBuffers();
}

// This function is called when the window size changes.
// w : is the new width of the window in pixels.
// h : is the new height of the window in pixels.
void onResize(int w, int h)
{
	// Record the new width & height
	m_windowWidth = w;
	m_windowHeight = h;

	// Adjust the window
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-w / 2, w / 2, -h / 2, h / 2, -1, 1); // Set origin to the center of the screen
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Refresh the view
	display();

	// Reshape back to original size
	glutReshapeWindow(TOTAL_WIDTH, TOTAL_HEIGHT);
}

//
// Special Key like GLUT_KEY_F1, F2, F3,...
// Arrow Keys, GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_RIGHT, GLUT_KEY_RIGHT
//
void onSpecialKeyDown(int key, int x, int y)
{
	// Write your codes here.
	switch (key) 
	{
		case GLUT_KEY_LEFT: 
			m_input_left = true; 
			break;

		case GLUT_KEY_RIGHT: 
			m_input_right = true; 
			break;
	}

	// to refresh the window it calls display() function
	glutPostRedisplay();
}

// Called on key release for ASCII characters (ex.: ESC, a,b,c... A,B,C...)
void onKeyUp(const unsigned char key, const int x, const int y)
{
	switch (key)
	{
		case 27: 
			exit(0); // Exit when ESC is pressed
			break;

		case ' ': 
			onSpacebarPressed(); 
			break;
	}

	// Refresh the view
	glutPostRedisplay();
}

// Special Key like GLUT_KEY_F1, F2, F3,...
// Arrow Keys, GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_RIGHT, GLUT_KEY_RIGHT
void onSpecialKeyUp(const int key, const int x, const int y)
{
	switch (key) 
	{
		case GLUT_KEY_F1: 
			onF1Pressed(); 
			break;

		case GLUT_KEY_LEFT: 
			m_input_left = false;
			break;

		case GLUT_KEY_RIGHT: 
			m_input_right = false;
			break;
	}

	// Refresh the view
	glutPostRedisplay();
}


// Timer tick receiver
// 'v' is the 'value' parameter passed to 'glutTimerFunc'
void onTimerTick_Game(const int v)
{
	// Reschedule the next timer tick
	glutTimerFunc(TIMER_PERIOD_GAME_UPDATE, onTimerTick_Game, 0);

	// Calculate the actual deltaTime
	int now = NOW;
	int deltaTime = now - m_lastTimerTickTime;
	m_lastTimerTickTime = now;

	if (m_currentGameState == running)
	{
		updateGame(deltaTime);
	}

	// Refresh the view
	glutPostRedisplay();
}





//
// Main
//

void main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(TOTAL_WIDTH, TOTAL_HEIGHT);
	glutCreateWindow("CTIS 164 Homework #2  |  S. Tarýk Çetin  |  Anti-Air Tank");

	glutDisplayFunc(display);
	glutReshapeFunc(onResize);

	// Keyboard input
	glutSpecialFunc(onSpecialKeyDown);

	glutKeyboardUpFunc(onKeyUp);
	glutSpecialUpFunc(onSpecialKeyUp);

	// prepare game
	m_currentGameState = notStarted;
	initializeNewSession();

	// Smoothing shapes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Schedule the initial timer tick
	glutTimerFunc(TIMER_PERIOD_GAME_UPDATE, onTimerTick_Game, 0);

	glutMainLoop();
}
