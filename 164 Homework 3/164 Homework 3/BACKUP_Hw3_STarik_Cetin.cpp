/*

CTIS164
----------
STUDENT : S. Tarýk Çetin
SECTION : 1
HOMEWORK: 3
----------
ADDITIONAL FEATURES:

- Aim line: A white line from gun to mouse pointer
- Aim point A small circle on mouse pointer

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

#define PI 3.1415
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
#define STRING_STATE_SHORT_FINISHED "Finished"

#define STRING_STATE_DETAILS_NOT_STARTED "Press Left Click to initiate the first session."
#define STRING_STATE_DETAILS_RUNNING "Press Left Click to to fire."
#define STRING_STATE_DETAILS_FINISHED "Press Left Click to initiate a new session."

//
// layout parameters

#define TOTAL_WIDTH			1000
#define TOTAL_HEIGHT		1000

//window
#define WINDOW_LEFT		-TOTAL_WIDTH / 2
#define WINDOW_RIGHT	-1 * WINDOW_LEFT
#define WINDOW_TOP		TOTAL_HEIGHT / 2
#define WINDOW_BOTTOM	-1 * WINDOW_TOP

//orbits
#define FIRST_ORBIT_RADIUS		250
#define ORBIT_RADIUS_INCREMENT	100

//celestials
#define CELESTIAL_MAX_RADIUS	50
#define CELESTIAL_MIN_RADIUS	10

//bullet
#define BULLET_RADIUS	5


//
// sizes and dimensions
//		Note: These are for the collision checkers.
//			  Altering these won't change the appearence of objects.
//			  To change appearence, take a look at the drawer methods.


//
// game parameters
#define ORBIT_COUNT		3

#define MAX_ANGULAR_VELOCITY	50
#define	MIN_ANGULAR_VELOCITY	-50

#define BULLET_SPEED	2000




//
// Custom Type Definitions
//

typedef enum { notStarted, running, finished } gameState_e;

typedef struct
{
	double x;
	double y;
} point_t;

typedef struct
{
	point_t position; // from the origin
	double rotation; // positive angle from the +x axis
} transform_t;

typedef struct
{
	double radius;
	double angle;
} polar_t;

typedef struct
{
	double colorR, colorG, colorB;
	double angularVel;
	double radius;

	point_t position;
	bool destroyed;
} celestial_t;

typedef struct
{
	point_t globalPos;
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





//
// Global Variables
//

int m_windowWidth, m_windowHeight;

int m_lastTimerTickTime;
gameState_e m_currentGameState = notStarted;

transform_t m_gunPivot;

point_t m_mousePos;

polar_t m_bulletPolar;
bool m_bulletInAir;

celestial_t m_celesitals[ORBIT_COUNT];





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
	float angle;
	glBegin(GL_POLYGON);
	for (int i = 0; i < 100; i++)
	{
		angle = 2 * PI*i / 100;
		glVertex2f(x + r*cos(angle), y + r*sin(angle));
	}
	glEnd();
}

// draws a circle wire (only the outline) with the center (x, y) and radius r
void drawCircleWire(const int x, const int y, const int r)
{
	float angle;
	glBegin(GL_LINE_LOOP);
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
void drawTriangle_horizontal(const point_t *center, const double width, const double height, const int lookDirection)
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
	case finished: return STRING_STATE_DETAILS_FINISHED;
	}

	return "---- error_string ----";
}

// returns 'point' rotated around origin by 'angle' degrees
point_t rotate(point_t point, double angle)
{
	double x = cos(angle*DEG_2_RAD) * point.x - sin(angle*DEG_2_RAD) * point.y;
	double y = sin(angle*DEG_2_RAD) * point.x + cos(angle*DEG_2_RAD) * point.y;
	point.x = x;
	point.y = y;
	return point;
}

double normalizeAngle(double angle)
{
	while (angle > 360)
	{
		angle -= 360;
	}

	while (angle < 0)
	{
		angle += 360;
	}

	return angle;
}

// returns rotation of 'point', from +x axis, in degrees
double rotation(point_t point)
{
	return normalizeAngle(atan2(point.y, point.x) / DEG_2_RAD);
}

point_t makePoint(double x, double y)
{
	point_t p;
	p.x = x;
	p.y = y;
	return p;
}

double mouseRotation()
{
	return rotation(m_mousePos);
}

void vertex(point_t p)
{
	glVertex2d(p.x, p.y);
}

point_t polartoCartesian(polar_t polar)
{
	point_t p;
	p.x = polar.radius * cos(polar.angle * DEG_2_RAD);
	p.y = polar.radius * sin(polar.angle * DEG_2_RAD);
	return p;
}

point_t difference(const point_t a, const point_t b)
{
	point_t res;
	res.x = b.x - a.x;
	res.y = b.y - a.y;
	return res;
}

double magnitude(const point_t p)
{
	return sqrtf(p.x * p.x + p.y * p.y);
}

double distance(const point_t a, const point_t b)
{
	return magnitude(difference(a, b));
}

bool circleCollision(const point_t aPos, const double aR, const point_t bPos, const double bR)
{
	double dist = distance(aPos, bPos);
	double rTotal = aR + bR;
	return dist <= rTotal;
}

polar_t cartesianToPolar(point_t p)
{
	polar_t pol;
	pol.angle = rotation(p);
	pol.radius = magnitude(p);
	return pol;
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
// position: the spawn position for the particle
particle_t* popExhaustParticle(particleSystem_t *ps, const point_t *position)
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

void aimBullet()
{
	m_bulletPolar.angle = mouseRotation();
}

void resetBulletToGun()
{
	m_bulletPolar.radius = 0;
}

bool checkCelestialHit(const celestial_t cel, const polar_t bullet)
{
	return !cel.destroyed && circleCollision(cel.position, cel.radius, polartoCartesian(m_bulletPolar), BULLET_RADIUS);
}

void checkAndHandleCollisions()
{
	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		if (checkCelestialHit(m_celesitals[i], m_bulletPolar))
		{
			// hit
			m_celesitals[i].destroyed = true;

			//m_bulletInAir = false;
			//resetBulletToGun();
		}
	}
}

void fire()
{
	if (!m_bulletInAir)
	{
		aimBullet();

		// release bullet
		m_bulletInAir = true;
	}
}

void updateCelestial(celestial_t *cel, double deltaTime)
{
	cel->position = rotate(cel->position, cel->angularVel * deltaTime / 1000);
}

void updateCelestials(const double deltaTime)
{
	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		updateCelestial(&m_celesitals[i], deltaTime);
	}
}

void updateBullet(double deltaTime)
{
	if (m_bulletInAir)
	{
		m_bulletPolar.radius += BULLET_SPEED * deltaTime / 1000;
	}
}

void checkAndHandleBulletOutOfBounds()
{
	point_t bulletPos = polartoCartesian(m_bulletPolar);

	if (bulletPos.x < WINDOW_LEFT || bulletPos.x > WINDOW_RIGHT || bulletPos.y < WINDOW_BOTTOM || bulletPos.y > WINDOW_TOP)
	{
		m_bulletInAir = false;
		resetBulletToGun();
	}
}

void endGame()
{
	m_currentGameState = finished;
}

void checkAndHandleEndOfGame()
{
	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		if (!m_celesitals[i].destroyed)
		{
			return;
		}
	}

	endGame();
}

// responsible for game update.
void updateGame(const double deltaTime)
{
	// deltatime is in milliseconds.

	updateCelestials(deltaTime);
	updateBullet(deltaTime);
	checkAndHandleCollisions();
	checkAndHandleBulletOutOfBounds();

	checkAndHandleEndOfGame();
}





//
// Initialization
//

void initData()
{
	m_bulletInAir = false;
	resetBulletToGun();
}

void initCelestial(celestial_t *celestial, int orbitRadius)
{
	// random color
	celestial->colorR = randomDouble(0, 255);
	celestial->colorG = randomDouble(0, 255);
	celestial->colorB = randomDouble(0, 255);

	// random angular velocity
	celestial->angularVel = randomDouble(MIN_ANGULAR_VELOCITY, MAX_ANGULAR_VELOCITY);

	// random position
	celestial->position = rotate(makePoint(orbitRadius, 0), randomDouble(0, 360));

	// random radius
	celestial->radius = randomDouble(CELESTIAL_MIN_RADIUS, CELESTIAL_MAX_RADIUS);

	// data
	celestial->destroyed = false;
}

void initCelestials()
{
	double nextRad = FIRST_ORBIT_RADIUS;

	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		initCelestial(&m_celesitals[i], nextRad);
		nextRad += ORBIT_RADIUS_INCREMENT;
	}
}

void initGameObjects()
{
	initCelestials();
}

// Seeds Rand, initializes Game Objects, and initializes Data. In that order.
void initNewSession()
{
	srand(time(0));

	initGameObjects();
	initData();
}




//
// Drawers
//

void drawAxes()
{
	// x axis
	glLineWidth(1);
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINES);
	glVertex2f(WINDOW_LEFT, 0);
	glVertex2f(WINDOW_RIGHT, 0);
	glEnd();

	// y axis
	glLineWidth(1);
	glColor3f(0.0, 1.0, 0.0);
	glBegin(GL_LINES);
	glVertex2f(0, WINDOW_TOP);
	glVertex2f(0, WINDOW_BOTTOM);
	glEnd();
}

void drawAxesLabels()
{
	// x labels
	glColor3f(1.0, 0.0, 0.0);
	for (int i = -9; i < 10; i++)
	{
		int x = i * (TOTAL_WIDTH / 2) / 10;
		drawBitmapString_WithVars(0, x, -15, GLUT_BITMAP_8_BY_13, "%d", x);
	}

	// y labels
	glColor3f(0.0, 1.0, 0.0);
	for (int i = -9; i < 10; i++)
	{
		int y = i * (TOTAL_WIDTH / 2) / 10;
		drawBitmapString_WithVars(1, -5, y - 5, GLUT_BITMAP_8_BY_13, "%d", y);
	}
}

void drawOrbitAtOrigin(int radius)
{
	glColor3ub(0, 125, 255);
	drawCircleWire(0, 0, radius);
}

void drawOrbits(int orbitCount, int firstOrbitRadius, int orbitRadiusIncrement)
{
	int nextRadius = firstOrbitRadius;

	for (int i = 0; i < orbitCount; i++)
	{
		drawOrbitAtOrigin(nextRadius);
		nextRadius += orbitRadiusIncrement;
	}
}

void drawAimLineAndPoint()
{
	// aim point
	glColor3ub(255, 255, 255);
	drawCircle(m_mousePos.x, m_mousePos.y, 5);

	// aim line
	glColor3ub(255, 255, 255);
	glBegin(GL_LINES);
	glVertex2d(0, 0);

	point_t end = m_mousePos;

	glVertex2d(end.x, end.y);
	glEnd();
}

void drawMouseRotation()
{
	glColor3ub(255, 255, 255);
	
	drawBitmapString_WithVars(0, m_mousePos.x + 50, m_mousePos.y, GLUT_BITMAP_8_BY_13, "%0.2f deg", mouseRotation());
}

void drawGameAreaBackground()
{
	drawAxes();
	drawOrbits(ORBIT_COUNT, FIRST_ORBIT_RADIUS, ORBIT_RADIUS_INCREMENT);
	drawAxesLabels();
	drawAimLineAndPoint();
}

void drawGun(transform_t pivot)
{
	// base diamond
	glColor3ub(255, 255, 255);
	glBegin(GL_POLYGON);
	glVertex2d(-25, 0);
	glVertex2d(0, 25);
	glVertex2d(25, 0);
	glVertex2d(0, -25);
	glEnd();

	// outer shell
	point_t oa = rotate(makePoint(0, 12), mouseRotation());
	point_t ob = rotate(makePoint(40, 12), mouseRotation());
	point_t oc = rotate(makePoint(75, 0), mouseRotation());
	point_t od = rotate(makePoint(40, -12), mouseRotation());
	point_t oe = rotate(makePoint(0, -12), mouseRotation());

	glColor3ub(125, 0, 255);
	glBegin(GL_POLYGON);
	vertex(oa);
	vertex(ob);
	vertex(oc);
	vertex(od);
	vertex(oe);
	glEnd();

	// inner triangle
	point_t ia = rotate(makePoint(100, 0), mouseRotation());
	point_t ib = rotate(makePoint(0, 12), mouseRotation());
	point_t ic = rotate(makePoint(-50, 0), mouseRotation());
	point_t id = rotate(makePoint(0, -12), mouseRotation());

	glColor3ub(0, 125, 255);
	glBegin(GL_POLYGON);
	vertex(ia);
	vertex(ib);
	vertex(ic);
	vertex(id);
	glEnd();
}

void drawCelestial(const celestial_t cel)
{
	if (cel.destroyed)
	{
		return;
	}

	glColor3ub(cel.colorR, cel.colorG, cel.colorB);
	drawCircle(cel.position.x, cel.position.y, cel.radius);

	glColor3ub(0, 255, 0);
	drawBitmapString_WithVars(0, cel.position.x, cel.position.y - cel.radius - 15, GLUT_BITMAP_8_BY_13, "%0.2f", rotation(cel.position));
}

void drawCelestials()
{
	if (m_currentGameState != running)
	{
		return;
	}

	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		drawCelestial(m_celesitals[i]);
	}
}

void drawBullet(const polar_t bulletPolar)
{
	if (m_currentGameState != running)
	{
		return;
	}

	glColor3ub(255, 0, 0);
	point_t p = polartoCartesian(bulletPolar);
	drawCircle(p.x, p.y, 10);
}

void drawGameObjects()
{
	drawGun(m_gunPivot);
	drawCelestials();
	drawBullet(m_bulletPolar);
}

void drawTitleTexts()
{
	glColor3ub(100, 255, 100);

	// title
	drawBitmapString(-1, WINDOW_LEFT + 25, WINDOW_TOP - 25, GLUT_BITMAP_8_BY_13, "Homework #3");

	// name
	drawBitmapString(-1, WINDOW_LEFT + 25, WINDOW_TOP - 45, GLUT_BITMAP_8_BY_13, "S. Tarik Cetin");
}

void drawStatusTexts()
{
	glColor3ub(100, 255, 100);

	// state
	drawBitmapString(1, WINDOW_RIGHT - 25, WINDOW_TOP - 25, GLUT_BITMAP_8_BY_13, getCurrentStateText_State());

	// detailes
	drawBitmapString(1, WINDOW_RIGHT - 25, WINDOW_TOP - 45, GLUT_BITMAP_8_BY_13, getCurrentStateText_Details());
}

void drawTexts()
{
	drawTitleTexts();
	drawStatusTexts();

	drawMouseRotation();
}

// Draws Statics, Game Objects, and Texts; in that order.
void draw()
{
	drawGameAreaBackground(); // game area background is drawn before so game objects are visible.
	drawGameObjects();
	drawTexts();
}





//
// GLUT Event Handlers
//

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

	//// Reshape back to original size
	//glutReshapeWindow(TOTAL_WIDTH, TOTAL_HEIGHT);
}

// Called on key release for ASCII characters (ex.: ESC, a,b,c... A,B,C...)
void onKeyUp(const unsigned char key, const int x, const int y)
{
	switch (key)
	{
	case 27:
		exit(0); // Exit when ESC is pressed
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

void handleMouseMove(int x, int y)
{
	//   x2 = x1 - winWidth / 2
	//   y2 = winHeight / 2 - y1
	int x2 = x - m_windowWidth / 2;
	int y2 = m_windowHeight / 2 - y;

	m_mousePos.x = x2;
	m_mousePos.y = y2;
}

void onMouseMove(int x, int y) 
{
	handleMouseMove(x, y);

	// to refresh the window it calls display() function   
	glutPostRedisplay();
}

// GLUT to OpenGL coordinate conversion:
//   x2 = x1 - winWidth / 2
//   y2 = winHeight / 2 - y1
void onMouseMovePassive(int x, int y) 
{
	handleMouseMove(x, y);

	// to refresh the window it calls display() function
	glutPostRedisplay();
}

void handleLeftClick()
{
	switch (m_currentGameState)
	{
	case notStarted:
	case finished:
		initNewSession();
		m_currentGameState = running;
		break;

	case running:
		fire();
		break;
	}
}

//
// When a click occurs in the window,
// It provides which button
// buttons : GLUT_LEFT_BUTTON , GLUT_RIGHT_BUTTON
// states  : GLUT_UP , GLUT_DOWN
// x, y is the coordinate of the point that mouse clicked.
//
void onClick(int button, int state, int x, int y)
{
	if (state != GLUT_UP)
	{
		return;
	}

	switch (button)
	{
	case 0:
		handleLeftClick();
		break;
	}

	// to refresh the window it calls display() function
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
	glutCreateWindow("CTIS 164 Homework #3  |  S. Tarýk Çetin  |  Orbital Defence");

	glutDisplayFunc(display);
	glutReshapeFunc(onResize);

	// Keyboard input
	glutKeyboardUpFunc(onKeyUp);

	// Mouse Input
	glutMouseFunc(onClick);
	glutMotionFunc(onMouseMove);
	glutPassiveMotionFunc(onMouseMovePassive);

	// prepare game
	m_currentGameState = notStarted;
	initNewSession();

	// Smoothing shapes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Schedule the initial timer tick
	glutTimerFunc(TIMER_PERIOD_GAME_UPDATE, onTimerTick_Game, 0);

	glutMainLoop();
}
