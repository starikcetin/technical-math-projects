#define GLUT_DISABLE_ATEXIT_HACK

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

#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 650

#define FIXED_WIDTH 1000
#define HEADER_HEIGHT 150
#define LANE_HEIGHT 100
#define LANE_SEPERATOR_HEIGHT 1

#define ROCKET_COUNT 5
#define ROCKET_WIDTH 160
#define ROCKET_HEIGHT 50

#define ROCKET_MAX_ACC 50
#define ROCKET_MIN_ACC 0
#define ROCKET_MAX_X_VELOCITY 400
#define ROCKET_WOBBLE_PERIOD 100 //in milliseconds
#define ROCKET_MAX_EXHAUST_PARTICLES 100
#define ROCKET_EXHAUST_PARTICLES_LIFESPAN 200
#define ROCKET_EXHAUST_PARTICLE_CREATE_COOLDOWN 10

#define TIMER_PERIOD_GAME_UPDATE 1
#define TIMER_PERIOD_ACC_UPDATE 500

#define STRING_STATE_SHORT_NOT_STARTED "Welcome"
#define STRING_STATE_SHORT_RUNNING "Running"
#define STRING_STATE_SHORT_PAUSED "Paused"
#define STRING_STATE_SHORT_FINISHED "Finished"

#define STRING_STATE_DETAILS_NOT_STARTED "Press F1 to initiate the first session."
#define STRING_STATE_DETAILS_RUNNING "Press Spacebar to pause the execution. Press F1 to initiate a new session."
#define STRING_STATE_DETAILS_PAUSED "Press Spacebar to resume the execution. Press F1 to initiate a new session."
#define STRING_STATE_DETAILS_FINISHED "Press F1 to initiate a new session."

// The time passed in milliseconds since the beginning of the application
#define NOW glutGet(GLUT_ELAPSED_TIME)




//
// Custom Type Definitions
//

typedef enum { none, left, right } edge_e;
typedef enum { ignore, flip, finish, impossible } touchResolution_e;
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
} exhaustParticle_t;

typedef struct
{
	// alive exhaust particles
	exhaustParticle_t particlePool[ROCKET_MAX_EXHAUST_PARTICLES];

	// the index of the particle created last
	int lastUsedParticleIndex;

	// last time an exhaust particle was created
	double lastParticleCreationTime;
} exhaustParticleSystem_t;

typedef struct 
{
	// this is used to introduce a shift to sine function in wobble, 
	// so the wobbling effect looks different in each rocket
	double wobbleRandomizer;

	// the position of the lane this rocket is in on Y axis.
	double laneMiddlePositionY;

	vector2_t globalPos;
	
	// 1 is positive direction
	// -1 is negative direction
	int lookDirection;

	// pixels per second
	// local coordinates
	vector2_t velocity;

	// pixels per second^2
	// local coordinates
	vector2_t acceleration;

	// the edge this rocket last touched to
	edge_e lastTouch;

	// total distance travelled in this game session
	double distanceTravelled;

	// particle system handling exhaust particles.
	exhaustParticleSystem_t exhaustParticleSystem;
} rocketTransform_t;




//
// Global Variables
//

int m_windowWidth, m_windowHeight;
rocketTransform_t m_rockets[ROCKET_COUNT];
int m_leaderIndex;
int m_lastTimerTime, m_sessionRunTime;
gameState_e m_currentGameState = notStarted;




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
	glRectd(left,		top, 
			left+width, top-height);
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
double headerToGlobal(const double y)
{
	// since we shift the lanes themselves by the height of header, 
	// calculating the center of the top lane as if header was not here 
	// will give us the center of header.
	return y + LANE_HEIGHT * (ROCKET_COUNT / 2.0);
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
}




//
// Particle System Methods
//

// returns a particle.
// ps: particle system
// isForUse: if true, 'lastUsedParticleIndex' will be incremented by 1.
exhaustParticle_t* getAvailableExhaustParticle(exhaustParticleSystem_t *ps, const bool isForUse)
{
	//for (int i = 0; i < ROCKET_MAX_EXHAUST_PARTICLES; i++)
	//{
	//	int lookToIndex = (i + lastUsedParticleIndex) % ROCKET_MAX_EXHAUST_PARTICLES;
	//
	//	if (!particlePool[lookToIndex].alive)
	//	{
	//		lastUsedParticleIndex = lookToIndex;
	//		return &particlePool[lookToIndex];
	//	}
	//}

	int toReturn = ps->lastUsedParticleIndex;

	if (isForUse)
	{
		ps->lastUsedParticleIndex = (ps->lastUsedParticleIndex + 1) % ROCKET_MAX_EXHAUST_PARTICLES;
	}

	return &(ps->particlePool[toReturn]);
}

// calculates and kills overdue particles
// ps: particle system
void killOverdueParticles(exhaustParticleSystem_t *ps)
{
	for (int i = 0; i < ROCKET_MAX_EXHAUST_PARTICLES; i++)
	{
		if (ps->particlePool[i].isAlive && ps->particlePool[i].aliveTime >= ROCKET_EXHAUST_PARTICLES_LIFESPAN)
		{
			// kill
			ps->particlePool[i].isAlive = false;
			ps->particlePool[i].aliveTime = 0;
		}
	}
}

// kills all particles in this particle system
// ps: particle system
void killAllParticles(exhaustParticleSystem_t *ps)
{
	for (int i = 0; i < ROCKET_MAX_EXHAUST_PARTICLES; i++)
	{
		// kill
		ps->particlePool[i].isAlive = false;
		ps->particlePool[i].aliveTime = 0;
	}
}

// uses the oldest particle to make a new exhaust particle; then returns the particle
exhaustParticle_t* popExhaustParticle(rocketTransform_t *rt)
{
	exhaustParticle_t *particle = getAvailableExhaustParticle(&(rt->exhaustParticleSystem), true);

	particle->aliveTime = 0;

	particle->globalPos.x = rt->globalPos.x - (rt->lookDirection * ROCKET_WIDTH / 2);
	particle->globalPos.y = rt->globalPos.y;
	
	particle->isAlive = true;

	return particle;
}




//
// Game Logic
//

// called on finish of any rocket
void onFinish()
{
	if (m_currentGameState == running)
	{
		// stop the updates.
		m_currentGameState = finished;
	}
	else
	{
		printf("*!* Impossible state transition attempt in onFinish function.\n");
	}
}

// returns the course of action for the resolution of the given edge touches
touchResolution_e processEdgeTouch(const edge_e oldT, const edge_e newT)
{
	if (oldT == left)
	{
		if (newT == right)
		{
			return impossible;
		}
	}
	else if (oldT == right)
	{
		if (newT == left)
		{
			return impossible;
		}
	}
	else if (oldT == none)
	{
		if (newT == left)
		{
			return finish;
		}
		else if (newT == right)
		{
			return flip;
		}
	}

	return ignore;
}

// returns the edge this rocket is touching or 'none'
edge_e detectEdgeTouch(const rocketTransform_t *rocket)
{
	if (rocket->globalPos.x + ROCKET_WIDTH/2 >= FIXED_WIDTH/2)
	{
		return right;
	}
	else if (rocket->globalPos.x - ROCKET_WIDTH / 2 <= -FIXED_WIDTH / 2)
	{
		return left;
	}
	
	// no touches
	return none;
}

// returns a random double in a range of ROCKET_MIN_ACC and ROCKET_MAX_ACC
double randomAcceleration()
{
	return randomDouble(ROCKET_MIN_ACC, ROCKET_MAX_ACC);
}

// assignes a random horizontal acceleration to all rockets
void updateAccelerations()
{
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		// Assign a random acceleration
		//m_rockets[i].velocity.x = randomSpeed();
		m_rockets[i].acceleration.x = randomAcceleration();

	}
}

// responsible for updating the exhaust system of a rocket
// rt: the rocket transform
void updateExhaust(rocketTransform_t *rt, const double deltaTime)
{
	// cleanup
	killOverdueParticles(&(rt->exhaustParticleSystem));
	//printf("kill\n");

	// create a new particle if it is the time
	if (NOW - rt->exhaustParticleSystem.lastParticleCreationTime > ROCKET_EXHAUST_PARTICLE_CREATE_COOLDOWN)
	{
		// reset the timer
		rt->exhaustParticleSystem.lastParticleCreationTime = NOW;

		// create the particle
		popExhaustParticle(rt);
		//printf("pop\n");

		//update all alive particles' aliveTime
		for (int i = 0; i < ROCKET_MAX_EXHAUST_PARTICLES; i++)
		{
			if (rt->exhaustParticleSystem.particlePool[i].isAlive)
			{
				rt->exhaustParticleSystem.particlePool[i].aliveTime += deltaTime;
			}
		}
	}
}

int temp = 0;

// responsible for game update.
void updateGame(const double deltaTime)
{
	// deltatime is in milliseconds.

	int leaderIndex = 0;

	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		//printf("\n%f", deltaTime); // temporary
		//printf("\nrand: %f", randomDouble(-1, 1)); // temporary

		// Process acceleration (accelerate)
		m_rockets[i].velocity.x += m_rockets[i].acceleration.x * deltaTime / 1000;
		m_rockets[i].velocity.y += m_rockets[i].acceleration.y * deltaTime / 1000;

		if (m_rockets[i].velocity.x > ROCKET_MAX_X_VELOCITY)
		{
			m_rockets[i].velocity.x = ROCKET_MAX_X_VELOCITY;
		}

		// Process velocity (move)
		m_rockets[i].globalPos.x += m_rockets[i].velocity.x * deltaTime / 1000 * m_rockets[i].lookDirection;
		m_rockets[i].globalPos.y += m_rockets[i].velocity.y * deltaTime / 1000 * m_rockets[i].lookDirection;

		// Update distance travelled
		m_rockets[i].distanceTravelled += m_rockets[i].velocity.x;

		// Detect leader
		if (m_rockets[i].distanceTravelled > m_rockets[leaderIndex].distanceTravelled)
		{
			leaderIndex = i;
		}

		// Detect edge touch
		edge_e touching = detectEdgeTouch(&m_rockets[i]);

		// Process edge touch
		touchResolution_e resolve = processEdgeTouch(m_rockets[i].lastTouch, touching);

		switch (resolve)
		{
		case finish:
			printf("finish %d | lastEdge: %d | touching: %d\n", i, m_rockets[i].lastTouch, touching);
			onFinish(); // react to the finish
			break;

		case flip:
			printf("flip %d | lastEdge: %d | touching: %d\n", i, m_rockets[i].lastTouch, touching);
			m_rockets[i].lookDirection *= -1; // flip the rocket
			break;

		case impossible:
			printf("** impossible %d | lastEdge: %d | touching: %d\n **", i, m_rockets[i].lastTouch, touching);
			break;

		//case ignore:
		//	break;
		}

		// Save edge touch
		m_rockets[i].lastTouch = touching;

		// Update the exhaust system
		updateExhaust(&m_rockets[i], deltaTime);

		// Wobble
		m_rockets[i].globalPos.y = 
			m_rockets[i].laneMiddlePositionY														// original lane position
			+ sin(m_rockets[i].wobbleRandomizer + (double)m_sessionRunTime / ROCKET_WOBBLE_PERIOD)	// sine value of time, with predefined period
			* (LANE_HEIGHT - ROCKET_HEIGHT - 40) / 2												// clamp in two texts above (acc) and below (spd)
			/ 100 * rangeConversion(m_rockets[i].velocity.x, 0, ROCKET_MAX_X_VELOCITY, 1, 100);		// scale with x velocity

		//printf("%d: %0.2f\n", i, m_rockets[i].globalPos.y);
	}

	//printf("----------\n");

	m_leaderIndex = leaderIndex;
	m_sessionRunTime += deltaTime;
}




//
// Initialization
//

void resetExhaustParticleSystem(exhaustParticleSystem_t *ps)
{
	ps->lastUsedParticleIndex = 0;
	ps->lastParticleCreationTime = 0;
	killAllParticles(ps); // force kill all particles
}

void initializeRockets()
{
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		// move to start
		m_rockets[i].globalPos.x = -FIXED_WIDTH / 2 + ROCKET_WIDTH / 2;

		// move to lanes
		m_rockets[i].laneMiddlePositionY = LANE_HEIGHT * (ROCKET_COUNT / 2.0 - i) - HEADER_HEIGHT / 2.0 - LANE_HEIGHT / 2.0;
		m_rockets[i].globalPos.y = m_rockets[i].laneMiddlePositionY;

		// look to positive
		m_rockets[i].lookDirection = 1;

		// set last touch to left
		m_rockets[i].lastTouch = left;

		// zero velocity and acceleration
		m_rockets[i].velocity.x = 0;
		m_rockets[i].velocity.y = 0;
		m_rockets[i].acceleration.x = 0;
		m_rockets[i].acceleration.y = 0;

		// zero road-o-meter
		m_rockets[i].distanceTravelled = 0;

		// particle system
		resetExhaustParticleSystem(&m_rockets[i].exhaustParticleSystem);

		// wobble randomizer
		m_rockets[i].wobbleRandomizer = rangeConversion(rand(), 0, RAND_MAX, -1, 1);
	}

	// assign initial accelerations
	updateAccelerations();
}

void initializeData()
{
	m_leaderIndex = 0;
	m_lastTimerTime = NOW;
	m_sessionRunTime = 0;
}

// Initailizes Rockets and Data, in that order.
void initializeNewSession()
{
	printf("---- New session ----\n");

	srand(time(0));

	initializeRockets();
	initializeData();
}




//
// Drawers
//

void drawRocket_Exhaust(const rocketTransform_t *rocket)
{
	for (int i = 0; i < ROCKET_MAX_EXHAUST_PARTICLES; i++)
	{
		exhaustParticle_t particle = rocket->exhaustParticleSystem.particlePool[i];

		if (particle.isAlive)
		{
			//double blueFromAcc = rangeConversion(rocket->acceleration.x, ROCKET_MIN_ACC, ROCKET_MAX_ACC, 0, 1);
			double greenFromTime = rangeConversion(particle.aliveTime, 0, ROCKET_EXHAUST_PARTICLES_LIFESPAN, 0, 1);
			double sizeFromTime = rangeConversion(particle.aliveTime, 0, ROCKET_EXHAUST_PARTICLES_LIFESPAN, 10, 1);

			glColor3d(1, greenFromTime, /*blueFromAcc*/ 0); // red -> yellow
			drawCircle(particle.globalPos.x, particle.globalPos.y, sizeFromTime);
		}
	}
}

void drawRocket_Tail(const rocketTransform_t *rocket)
{
	// 5-Poly
	// (-80, 25)
	// (-55, 25)
	// (-30, 0)
	// (-55, -25)
	// (-80, -25)

	glColor3d(0.15, 0.15, 0.2); // dusty black

	glBegin(GL_POLYGON);
	glVertex2d(-80 * rocket->lookDirection + rocket->globalPos.x,	25 + rocket->globalPos.y);
	glVertex2d(-55 * rocket->lookDirection + rocket->globalPos.x,	25 + rocket->globalPos.y);
	glVertex2d(-30 * rocket->lookDirection + rocket->globalPos.x,	0 + rocket->globalPos.y);
	glVertex2d(-55 * rocket->lookDirection + rocket->globalPos.x,	-25 + rocket->globalPos.y);
	glVertex2d(-80 * rocket->lookDirection + rocket->globalPos.x,	-25 + rocket->globalPos.y);
	glEnd();
}

void drawRocket_Body(const rocketTransform_t *rocket)
{
	// Rectangle
	// (-80, 10)	(t-l)*
	// (55, 10)		(t-r)
	// (55, -10)	(b-r)*
	// (-80, -10)	(b-l)

	glColor3d(0.9, 0.85, 0.9); // dusty white

	glRectd(-80 * rocket->lookDirection + rocket->globalPos.x,	10 + rocket->globalPos.y,
			55 * rocket->lookDirection + rocket->globalPos.x,	-10 + rocket->globalPos.y);
}

void drawRocket_PilotCabin(const rocketTransform_t *rocket)
{
	// 4-Poly
	// (-15, 10)
	// (30, 14)
	// (40, 14)
	// (50, 10)

	glColor3d(0.1, 0.3, 0.5); // glass blue + black

	glBegin(GL_POLYGON);
	glVertex2d(-15 * rocket->lookDirection + rocket->globalPos.x,	10 + rocket->globalPos.y);
	glVertex2d(30 * rocket->lookDirection + rocket->globalPos.x,	14 + rocket->globalPos.y);
	glVertex2d(40 * rocket->lookDirection + rocket->globalPos.x,	14 + rocket->globalPos.y);
	glVertex2d(50 * rocket->lookDirection + rocket->globalPos.x,	10 + rocket->globalPos.y);
	glEnd();
}

void drawRocket_NoseCone(const rocketTransform_t *rocket)
{
	// Triangle
	// (55, 10)
	// (80, 0)
	// (55, -10)

	glColor3d(0.65, 0, 0); // maroon

	glBegin(GL_POLYGON);
	glVertex2d(55 * rocket->lookDirection + rocket->globalPos.x,	10 + rocket->globalPos.y); //top
	glVertex2d(80 * rocket->lookDirection + rocket->globalPos.x,	0 + rocket->globalPos.y); //head
	glVertex2d(55 * rocket->lookDirection + rocket->globalPos.x,	-10 + rocket->globalPos.y); //bottom
	glEnd();
}

void drawRocket_RadarAntenna(const rocketTransform_t *rocket)
{
	// Rectangle
	// (-17, 15)	(t-l)*
	// (-16, 15)	(t-r)
	// (-16, 10)	(b-r)*
	// (-17, 10)	(b-l)

	glColor3d(0, 0, 0); // pure black

	glRectd(-18 * rocket->lookDirection + rocket->globalPos.x, 20 + rocket->globalPos.y,
			-16 * rocket->lookDirection + rocket->globalPos.x, 10 + rocket->globalPos.y);
}

void drawRocket_CargoHinge(const rocketTransform_t *rocket)
{
	// Circle

	glColor3d(0, 0, 0); // pale yellow
	drawCircle(-55 * rocket->lookDirection + rocket->globalPos.x, -2 + rocket->globalPos.y, 2);

	glColor3d(0, 0, 0); // pale yellow
	drawCircle(-45 * rocket->lookDirection + rocket->globalPos.x, -2 + rocket->globalPos.y, 2);

}

void drawRocket_Wing(const rocketTransform_t *rocket)
{
	// Rectangle

	glColor3d(0, 0, 0); // pure black

	glRectd(-70 * rocket->lookDirection + rocket->globalPos.x, 1 + rocket->globalPos.y,
		-30 * rocket->lookDirection + rocket->globalPos.x, -1 + rocket->globalPos.y);
}

void drawRocket_RadioAntenna(const rocketTransform_t *rocket)
{
	// Rectangle
	// (-17, 15)	(t-l)*
	// (-16, 15)	(t-r)
	// (-16, 10)	(b-r)*
	// (-17, 10)	(b-l)

	glColor3d(0.65, 0, 0); // maroon

	glRectd(-24 * rocket->lookDirection + rocket->globalPos.x, 20 + rocket->globalPos.y,
		-22 * rocket->lookDirection + rocket->globalPos.x, 10 + rocket->globalPos.y);
}

void drawRocket_Texts(const int rocketIndex)
{
	glColor3d(0, 0, 0); // pure black

	drawBitmapString_WithVars(0, 
		7 * m_rockets[rocketIndex].lookDirection + m_rockets[rocketIndex].globalPos.x, 
		-5 + m_rockets[rocketIndex].globalPos.y, 
		GLUT_BITMAP_8_BY_13,
		"X-15 #%d", rocketIndex);
}

void drawRocket_InfoPanel(const rocketTransform_t *rocket)
{
	//
	// Shapes
	//
	//glColor3d(0.7, 0, 0); // dark grey
	//glLineWidth(1);

	//glBegin(GL_LINES);
	//glVertex2d(-50 + rocket->globalPos.x, rocket->globalPos.y + 20);
	//glVertex2d(-50 + rocket->globalPos.x, rocket->globalPos.y - 20);
	//glEnd();

	//glLineWidth(1);

	//glBegin(GL_LINES);
	//glVertex2d(50 + rocket->globalPos.x, rocket->globalPos.y + 20);
	//glVertex2d(50 + rocket->globalPos.x, rocket->globalPos.y - 20);
	//glEnd();


	////
	//// Display
	////
	//drawString(-1, -65 + rocket->globalPos.x, LANE_HEIGHT/2 - 20 + rocket->globalPos.y,
	//	GLUT_BITMAP_8_BY_13, "ACC");
	//drawString_WithVars(1, 65 + rocket->globalPos.x, LANE_HEIGHT / 2 - 20 + rocket->globalPos.y,
	//	GLUT_BITMAP_8_BY_13, "%0.2f px/s^2", rocket->acceleration.x);

	//drawString(-1, -65 + rocket->globalPos.x, -LANE_HEIGHT/2 + 10 + rocket->globalPos.y,
	//	GLUT_BITMAP_8_BY_13, "SPD");
	//drawString_WithVars(1, 65 + rocket->globalPos.x, -LANE_HEIGHT / 2 + 10 + rocket->globalPos.y,
	//	GLUT_BITMAP_8_BY_13, "%0.2f px/s", rocket->velocity.x);

	
	//
	// Display
	//
	drawBitmapString(-1, -65 + rocket->globalPos.x, LANE_HEIGHT/2 - 20 + rocket->laneMiddlePositionY,
		GLUT_BITMAP_8_BY_13, "ACC");
	drawBitmapString_WithVars(1, 65 + rocket->globalPos.x, LANE_HEIGHT / 2 - 20 + rocket->laneMiddlePositionY,
		GLUT_BITMAP_8_BY_13, "%0.2f px/s^2", rocket->acceleration.x);

	drawBitmapString(-1, -65 + rocket->globalPos.x, -LANE_HEIGHT/2 + 10 + rocket->laneMiddlePositionY,
		GLUT_BITMAP_8_BY_13, "SPD");
	drawBitmapString_WithVars(1, 65 + rocket->globalPos.x, -LANE_HEIGHT / 2 + 10 + rocket->laneMiddlePositionY,
		GLUT_BITMAP_8_BY_13, "%0.2f px/s", rocket->velocity.x);
}

void drawRocket(const int rocketIndex)
{
	// Particles
	drawRocket_Exhaust(&m_rockets[rocketIndex]);

	// Shape
	drawRocket_Tail(&m_rockets[rocketIndex]);
	drawRocket_Body(&m_rockets[rocketIndex]);
	drawRocket_PilotCabin(&m_rockets[rocketIndex]);
	drawRocket_NoseCone(&m_rockets[rocketIndex]);

	// Details
	drawRocket_RadarAntenna(&m_rockets[rocketIndex]);
	drawRocket_CargoHinge(&m_rockets[rocketIndex]);
	drawRocket_Wing(&m_rockets[rocketIndex]);
	drawRocket_RadioAntenna(&m_rockets[rocketIndex]);

	// Displays
	drawRocket_Texts(rocketIndex);
	drawRocket_InfoPanel(&m_rockets[rocketIndex]);
}

void drawStatics()
{
	//
	// Header
	//

	glColor3d(0.2, 0.2, 0.2); // dark grey

	// background
	drawRectd_LTWH(	-FIXED_WIDTH / 2.0,
					(LANE_HEIGHT * ROCKET_COUNT / 2.0) + HEADER_HEIGHT / 2.0,
					FIXED_WIDTH,
					HEADER_HEIGHT);


	//
	// Game Area
	//

	// background
	//glColor3d(1, 1, 1); // white
	//drawRectd_LTWH(	-FIXED_WIDTH / 2.0,
	//				(LANE_HEIGHT * ROCKET_COUNT / 2.0) - HEADER_HEIGHT / 2.0,
	//				FIXED_WIDTH,
	//				ROCKET_COUNT * LANE_HEIGHT);


	// Top - Middle
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_POLYGON);

	glColor3d(0, 0.5, 1); // dark sky blue
	glVertex2d(-FIXED_WIDTH / 2, (LANE_HEIGHT * ROCKET_COUNT / 2.0) - HEADER_HEIGHT / 2); // top-left

	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(-FIXED_WIDTH / 2, -HEADER_HEIGHT / 2.0); // middle-left

	//glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(FIXED_WIDTH / 2, -HEADER_HEIGHT / 2.0); // middle-right

	glColor3d(0, 0.5, 1); // dark sky blue
	glVertex2d(FIXED_WIDTH / 2, (LANE_HEIGHT * ROCKET_COUNT / 2.0) - HEADER_HEIGHT / 2); // top-right
	glEnd();


	// Middle - Bottom
	glBegin(GL_POLYGON);
	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(-FIXED_WIDTH / 2, -HEADER_HEIGHT / 2.0); // middle-left

	glColor3d(0.3, 0.8, 1); // sky blue
	glVertex2d(-FIXED_WIDTH / 2, -(LANE_HEIGHT * ROCKET_COUNT / 2.0) - HEADER_HEIGHT / 2); // bottom-left
	
	//glColor3d(0.3, 0.8, 1); // sky blue
	glVertex2d(FIXED_WIDTH / 2, -(LANE_HEIGHT * ROCKET_COUNT / 2.0) - HEADER_HEIGHT / 2); // bottom-right

	glColor3d(0.7, 0.9, 1); // white + sky blue
	glVertex2d(FIXED_WIDTH / 2, -HEADER_HEIGHT / 2.0); // middle-right
	glEnd();


	//
	// Lanes
	//

	glColor3d(0.3, 0.3, 0.3); // dark grey

	for (int i = 1; i < ROCKET_COUNT; i++) // beginning and end doesn't need seperators (therefore starts from 1 and excludes last indice)
	{
		// Lane Seperator
		drawRectd_LTWH(	-FIXED_WIDTH/2, 
						LANE_HEIGHT * (ROCKET_COUNT / 2 - i) + LANE_HEIGHT / 2 - HEADER_HEIGHT / 2,
						FIXED_WIDTH, 
						LANE_SEPERATOR_HEIGHT);
	}
}

void drawRockets()
{
	for (int i = 0; i < ROCKET_COUNT; i++)
	{
		drawRocket(i);
	}
}

// draws all gameplay elements not related to rockets
void drawOtherGameplay()
{
	//
	// Leader line
	//

	double leaderX = m_rockets[m_leaderIndex].globalPos.x;
	double leaderFront = leaderX + (ROCKET_WIDTH / 2) * m_rockets[m_leaderIndex].lookDirection;

	glColor3d(1, 1, 0); //yellow
	glLineWidth(1);

	glBegin(GL_LINES);
	glVertex2d(leaderFront, ROCKET_COUNT*LANE_HEIGHT/2 - HEADER_HEIGHT/2);
	glVertex2d(leaderFront, -ROCKET_COUNT*LANE_HEIGHT/2 - HEADER_HEIGHT/2);
	glEnd();
}

void drawTexts()
{
	double lateralShift = 200;

	//
	// Header / Title
	//

	glColor3d(0.4, 1, 0.65); // pale green
	drawStrokeString_WithVars(0, 0, headerToGlobal(35), GLUT_STROKE_ROMAN, 0.2, "Racing X-15s"); // title label

	//glColor3d(0.4, 1, 0.65); // pale green
	drawStrokeString_WithVars(0, 0, headerToGlobal(0), GLUT_STROKE_ROMAN, 0.12, "S. Tarik Cetin"); // signature label

	//
	// Header / Time
	//

	glColor3d(1, 1, 1); // white
	drawBitmapString(-1, -190, headerToGlobal(-5), GLUT_BITMAP_8_BY_13, "Time"); // timer label

	// timer display
	drawBitmapString_WithVars(-1, -190, headerToGlobal(-19), GLUT_BITMAP_8_BY_13, 
		"%02d:%02d:%03d", 
		(m_sessionRunTime / 60000), 
		(m_sessionRunTime / 1000) % 60, 
		m_sessionRunTime % 1000);

	//
	// Header / Leader
	//

	glColor3d(1, 1, 1); // white
	drawBitmapString(1, 190, headerToGlobal(-5), GLUT_BITMAP_8_BY_13, "Leader"); // leader label

	// leader display
	switch (m_currentGameState)
	{
	case notStarted:
		glColor3d(1, 1, 1); // white
		drawBitmapString(1, 190, headerToGlobal(-19), GLUT_BITMAP_8_BY_13, "..."); 
		break;

	case finished:
		glColor3d(0.3, 1, 0.3); // fine green
		drawBitmapString_WithVars(1, 190, headerToGlobal(-19), GLUT_BITMAP_8_BY_13, "Winner is #%d", m_leaderIndex);
		break;

	case running:
	case paused:
		glColor3d(1, 1, 0.3); // fine yellow
		drawBitmapString_WithVars(1, 190, headerToGlobal(-19), GLUT_BITMAP_8_BY_13, "#%d", m_leaderIndex);
		break;
	}

	//
	// Header / Status
	//

	glColor3d(1, 1, 1); // white
	drawBitmapString(0, 0, headerToGlobal(-45), GLUT_BITMAP_8_BY_13, getCurrentStateText_State()); // status state
	drawBitmapString(0, 0, headerToGlobal(-60), GLUT_BITMAP_8_BY_13, getCurrentStateText_Details()); // status details
}

// Draws Statics, Rockets, and Texts; in that order.
void draw()
{
	//printf("\ndraw call"); // temporary
	drawStatics();
	drawRockets();
	drawOtherGameplay();
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
		initializeNewSession();
		m_currentGameState = running;
		break;

	case paused:
		initializeNewSession();
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
	switch (m_currentGameState)
	{
	case notStarted:
		break;

	case running:
		m_currentGameState = paused;
		break;

	case paused:
		m_currentGameState = running;
		break;

	case finished:
		break;

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
	glOrtho(-w/2, w/2, -h/2, h/2, -1, 1); // Set origin to the center of the screen
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Refresh the view
	display();
}

// Called on key release for ASCII characters (ex.: ESC, a,b,c... A,B,C...)
void onKeyUp(const unsigned char key, const int x, const int y)
{
	// Exit when ESC is pressed
	switch (key)
	{
	case 27: exit(0); break;
	case ' ': onSpacebarPressed(); break;
	}

	if (key == 27)
		exit(0);

	// Refresh the view
	glutPostRedisplay();
}

// Special Key like GLUT_KEY_F1, F2, F3,...
// Arrow Keys, GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_RIGHT, GLUT_KEY_RIGHT
void onSpecialKeyUp(const int key, const int x, const int y)
{
	switch (key) {
	case GLUT_KEY_F1: onF1Pressed(); break;
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
	int deltaTime = now - m_lastTimerTime;
	m_lastTimerTime = now;

	if (m_currentGameState == running)
	{
		updateGame(deltaTime);
	}

	// Refresh the view
	glutPostRedisplay();
}

// Timer tick receiver
// 'v' is the 'value' parameter passed to 'glutTimerFunc'
void onTimerTick_Velocity(const int v)
{
	// Reschedule the next timer tick
	glutTimerFunc(TIMER_PERIOD_ACC_UPDATE, onTimerTick_Velocity, 0);

	if (m_currentGameState == running)
	{
		updateAccelerations();
	}

	// no display refresh needed
}

//// GLUT to OpenGL coordinate conversion:
////   x2 = x1 - winWidth / 2
////   y2 = winHeight / 2 - y1
//void onMove(int x, int y) {
////	printf("%d - %d\n", x, y); // temporary
////	printf("%d - %d\n", x - m_windowWidth / 2, m_windowHeight / 2 - y); // temporary
//
//	// to refresh the window it calls display() function
//	//glutPostRedisplay();
//}




//
// Main
//

int main(int argc, char *argv[]) 
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
	glutCreateWindow("CTIS 164 Homework #1  |  Racing X-15s");

	glutDisplayFunc(display);
	glutReshapeFunc(onResize);

	// Keyboard input
	glutKeyboardUpFunc(onKeyUp);
	glutSpecialUpFunc(onSpecialKeyUp);

	//// Mouse input
	//glutPassiveMotionFunc(onMove);

	// Schedule the initial timer tick
	glutTimerFunc(TIMER_PERIOD_GAME_UPDATE, onTimerTick_Game, 0);
	glutTimerFunc(TIMER_PERIOD_ACC_UPDATE, onTimerTick_Velocity, 0);

	// Fire up
	m_currentGameState = notStarted;
	initializeNewSession();
	glutMainLoop();
	
	return 0;
}
