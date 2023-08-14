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

#define V_D2R 0.0174532
#define V_R2D 57.29608323
#define PI 3.1415

// The time passed in milliseconds since the beginning of the application
#define NOW glutGet(GLUT_ELAPSED_TIME)

//
// system parameters

#define TIMER_PERIOD	10

//
// layout parameters

#define TOTAL_WIDTH		800
#define TOTAL_HEIGHT	800

#define WINDOW_LEFT		-TOTAL_WIDTH / 2
#define WINDOW_RIGHT	-1 * WINDOW_LEFT
#define WINDOW_TOP		TOTAL_HEIGHT / 2
#define WINDOW_BOTTOM	-1 * WINDOW_TOP

#define BOTTOM_BAR_HEIGHT		50

#define GAME_AREA_BOTTOM		(WINDOW_BOTTOM + BOTTOM_BAR_HEIGHT)

#define GAME_AREA_MIN			{WINDOW_LEFT, GAME_AREA_BOTTOM}
#define GAME_AREA_MAX			{WINDOW_RIGHT, WINDOW_TOP}

#define FIRST_ORBIT_RADIUS		100
#define ORBIT_RADIUS_INCREMENT	100

#define PLANET_MAX_RADIUS	50
#define PLANET_MIN_RADIUS	10

#define SUN_RADIUS		20
#define LIGHT_RADIUS	10

#define UNLIT_GRAY		{0.1f, 0.1f, 0.1f}
#define UNLIT_BLACK		{0.0f, 0.0f, 0.0f}

//
// game parameters

#define ORBIT_COUNT		3

#define MAX_ANGULAR_VELOCITY	50
#define	MIN_ANGULAR_VELOCITY	-50

#define MIN_LIGHT_SPEED			100
#define MAX_LIGHT_SPEED			500

#define INITIAL_LIGHT_RANGE		500



//
// Custom Type Definitions
//

typedef struct {
	double x, y;
} vec_t;

typedef struct {
	double magnitude, angle;
} polar_t;

typedef struct
{
	double angularVel;
	double radius;
	vec_t position;
} planet_t;

typedef struct
{
	float r, g, b;
} color_t;

typedef struct {
	vec_t pos;
	vec_t normal;
} vertex_t;

typedef struct
{
	bool active;
	color_t color;
	double radius;
	vec_t position;
	vec_t velocity;
} light_t;





//
// Global Variables
//

int m_windowWidth, m_windowHeight;

int m_lastTimerTickTime;
bool m_running;
bool m_unlitIsBlack;

planet_t m_planets[ORBIT_COUNT];
light_t m_lights[4]; // 0 sun | 1 red | 2 green | 3 blue
double m_lightRange = INITIAL_LIGHT_RANGE;






//
// Utility Functions
//

double magV(vec_t v) {
	return sqrt(v.x * v.x + v.y * v.y);
}

double angleV(vec_t v) {
	double angle = atan2(v.y, v.x) * V_R2D;
	return angle < 0 ? angle + 360 : angle;
}

vec_t addV(vec_t v1, vec_t v2) {
	return{ v1.x + v2.x, v1.y + v2.y };
}

vec_t subV(vec_t v1, vec_t v2) {
	return{ v1.x - v2.x, v1.y - v2.y };
}

vec_t mulV(double k, vec_t v) {
	return{ k * v.x, k * v.y };
}

double dotP(vec_t v1, vec_t v2) {
	return v1.x * v2.x + v1.y * v2.y;
}

vec_t unitV(vec_t v) {
	return mulV(1.0 / magV(v), v);
}

// convert from polar representation to rectangular representation
vec_t pol2rec(polar_t p) {
	return{ p.magnitude * cos(p.angle * V_D2R),  p.magnitude * sin(p.angle * V_D2R) };
}

polar_t rec2pol(vec_t v) {
	return{ magV(v), angleV(v) };
}

double angleBetween2V(vec_t v1, vec_t v2) {
	double magV1 = magV(v1);
	double magV2 = magV(v2);
	double dot = dotP(v1, v2);
	double angle = acos(dot / (magV1 * magV2)) * V_R2D; // in degree
	return angle;
}

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

// returns a random double in a range of (min-max)
// no safety checks, be vary of overflows
double randomDouble(const double min, const double max)
{
	return rangeConversion(rand(), 0, RAND_MAX, min, max);
}

// returns 'vector' rotated by 'angle' degrees
vec_t rotate(vec_t vector, double angle)
{
	double x = cos(angle*V_D2R) * vector.x - sin(angle*V_D2R) * vector.y;
	double y = sin(angle*V_D2R) * vector.x + cos(angle*V_D2R) * vector.y;
	vector.x = x;
	vector.y = y;
	return vector;
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

// returns rotation of 'vector', from +x axis, in degrees
double rotation(vec_t vector)
{
	return normalizeAngle(atan2(vector.y, vector.x) / V_D2R);
}

vec_t makeVector(double x, double y)
{
	vec_t v;
	v.x = x;
	v.y = y;
	return v;
}

double clamp(double val, double min, double max)
{
	if (val < min)
	{
		return min;
	}
	else if (val > max)
	{
		return max;
	}
	else
	{
		return val;
	}
}

void setVertex(vec_t vector)
{
	glVertex2d(vector.x, vector.y);
}

void setColor(color_t c)
{
	glColor3f(c.r, c.g, c.b);
}

color_t mulColor(float k, color_t c) {
	color_t tmp = { k * c.r, k * c.g, k * c.b };
	return tmp;
}

color_t addColor(color_t c1, color_t c2) {
	color_t tmp = { c1.r + c2.r, c1.g + c2.g, c1.b + c2.b };
	return tmp;
}

// To add distance into calculation
// when distance is 0  => its impact is 1.0
// when distance is 350 => impact is 0.0
// Linear impact of distance on light calculation.
double distanceImpact(double d) {
	return (-1.0 / m_lightRange) * d + 1.0;
}

double clampAbove0(double value)
{
	return value < 0 ? 0 : value;
}

color_t calculateColor(light_t source, vertex_t v) {
	vec_t dist = subV(source.position, v.pos);
	vec_t distNormalized = unitV(dist);
	float factor = clampAbove0(dotP(distNormalized, v.normal)) * clampAbove0(distanceImpact(magV(dist)));
	return mulColor(factor, source.color);
}

color_t calculateTotalColor(vertex_t v)
{
	color_t buffer = { 0, 0, 0 };

	for (int i = 0; i < 4; i++)
	{
		if (m_lights[i].active)
		{
			buffer = addColor(buffer, calculateColor(m_lights[i], v));
		}
	}

	return buffer;
}




//
// Game Logic
//

void updatePlanet(planet_t *cel, double deltaTime)
{
	cel->position = rotate(cel->position, cel->angularVel * deltaTime / 1000);
}

void updatePlanets(const double deltaTime)
{
	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		updatePlanet(&m_planets[i], deltaTime);
	}
}

void updateLights(const double deltaTime)
{
	for (int i = 0; i < 4; i++)
	{
		m_lights[i].position = addV(m_lights[i].position, mulV(deltaTime / 1000, m_lights[i].velocity));
	}
}

// left or right
bool checkLightVertCol(const light_t light)
{
	double x = light.position.x;
	double r = light.radius;

	if (x - r <= WINDOW_LEFT || x + r >= WINDOW_RIGHT)
	{
		return true;
	}

	return false;
}

// top or bottom
bool checkLightHorzCol(const light_t light)
{
	double y = light.position.y;
	double r = light.radius;

	if (y - r <= GAME_AREA_BOTTOM || y + r >= WINDOW_TOP)
	{
		return true;
	}

	return false;
}

void checkAndHandleLightsCollisions()
{
	for (int i = 0; i < 4; i++)
	{
		if (checkLightVertCol(m_lights[i]))
		{
			m_lights[i].velocity.x *= -1;
			m_lights[i].position.x = clamp(m_lights[i].position.x, WINDOW_LEFT + m_lights[i].radius, WINDOW_RIGHT - m_lights[i].radius);
		}

		if (checkLightHorzCol(m_lights[i]))
		{
			m_lights[i].velocity.y *= -1;
			m_lights[i].position.y = clamp(m_lights[i].position.y, GAME_AREA_BOTTOM + m_lights[i].radius, WINDOW_TOP - m_lights[i].radius);
		}
	}
}

// responsible for game update.
void updateGame(const double deltaTime)
{
	// deltatime is in milliseconds.

	updatePlanets(deltaTime);
	checkAndHandleLightsCollisions();
	updateLights(deltaTime);
}





//
// Initialization
//

vec_t randVect(vec_t min, vec_t max)
{
	return { randomDouble(min.x, max.x), randomDouble(min.y, max.y) };
}

vec_t randomLightVelocity()
{
	polar_t p;
	p.magnitude = randomDouble(MIN_LIGHT_SPEED, MAX_LIGHT_SPEED);
	p.angle = randomDouble(0, 360);
	return pol2rec(p);
}

void initLights()
{
	m_lights[0].active = true;
	m_lights[0].position = { 0, 0 };
	m_lights[0].velocity = { 0, 0 };
	m_lights[0].radius = SUN_RADIUS;

	for (int i = 1; i < 4; i++)
	{
		m_lights[i].active = true;
		m_lights[i].position = randVect(GAME_AREA_MIN, GAME_AREA_MAX);
		m_lights[i].velocity = randomLightVelocity();
		m_lights[i].radius = LIGHT_RADIUS;
	}

	m_lights[0].color = { 1, 1, 1 };
	m_lights[1].color = { 1, 0, 0 };
	m_lights[2].color = { 0, 1, 0 };
	m_lights[3].color = { 0, 0, 1 };
}

void initPlanet(planet_t *planet, int orbitRadius)
{
	// random angular velocity
	planet->angularVel = randomDouble(MIN_ANGULAR_VELOCITY, MAX_ANGULAR_VELOCITY);

	// random position
	planet->position = rotate(makeVector(orbitRadius, 0), randomDouble(0, 360));

	// random radius
	planet->radius = randomDouble(PLANET_MIN_RADIUS, PLANET_MAX_RADIUS);
}

void initPlanets()
{
	double nextRad = FIRST_ORBIT_RADIUS;

	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		initPlanet(&m_planets[i], nextRad);
		nextRad += ORBIT_RADIUS_INCREMENT;
	}
}

// Seeds Rand, initializes Game Objects, and initializes Data. In that order.
void initNewSession()
{
	initPlanets();
	initLights();
	m_lightRange = INITIAL_LIGHT_RANGE;
	m_unlitIsBlack = false;
}




//
// Drawers
//

void drawBottomBarBackground()
{
	glColor3ub(25, 25, 25);
	glRectd(-400, GAME_AREA_BOTTOM, 400, GAME_AREA_BOTTOM - BOTTOM_BAR_HEIGHT);
}

void drawBackgrounds()
{
	drawBottomBarBackground();
}

void drawPlanet(const planet_t cel)
{
	glBegin(GL_TRIANGLE_FAN);

	if (m_unlitIsBlack)
	{
		setColor(UNLIT_BLACK);
	}
	else
	{
		setColor(UNLIT_GRAY);
	}

	setVertex(cel.position); //center

	polar_t relVertPolar;
	relVertPolar.angle = 0;
	relVertPolar.magnitude = cel.radius;

	while (relVertPolar.angle <= 360)
	{
		vec_t relVertPos = pol2rec(relVertPolar);
		vec_t vertPos = addV(relVertPos, cel.position);

		vertex_t vert;
		vert.pos = vertPos;
		vert.normal = unitV(subV(vertPos, cel.position));
		
		setColor(calculateTotalColor(vert));
		setVertex(vert.pos);
		relVertPolar.angle++;
	}

	glEnd();
}

void drawPlanets()
{
	for (int i = 0; i < ORBIT_COUNT; i++)
	{
		drawPlanet(m_planets[i]);
	}
}

void drawLights()
{
	for (int i = 0; i < 4; i++)
	{
		if (m_lights[i].active)
		{
			setColor(m_lights[i].color);
		}
		else
		{
			if (m_unlitIsBlack)
			{
				setColor(UNLIT_BLACK);
			}
			else
			{
				setColor(UNLIT_GRAY);
			}
		}

		drawCircle(m_lights[i].position.x, m_lights[i].position.y, m_lights[i].radius);
	}
}

void drawGameObjects()
{
	drawPlanets();
	drawLights();
}

void drawTitleTexts()
{
	glColor3ub(100, 255, 100);

	// title
	drawBitmapString(-1, WINDOW_LEFT + 25, WINDOW_TOP - 25, GLUT_BITMAP_8_BY_13, "Homework #4");

	// name
	drawBitmapString(-1, WINDOW_LEFT + 25, WINDOW_TOP - 45, GLUT_BITMAP_8_BY_13, "S. Tarik Cetin");
}

void drawStatusTexts()
{
	color_t red = { 1, 0.1, 0.1 };
	color_t green = { 0.1, 1, 0.1 };
	color_t blue = { 0.1, 1, 1 };

	setColor(m_lights[0].active ? green : red);
	drawBitmapString_WithVars(-1, -390, -380, GLUT_BITMAP_8_BY_13, "F1(Sun): %s", m_lights[0].active ? "on" : "off");

	setColor(m_lights[1].active ? green : red);
	drawBitmapString_WithVars(-1, -265, -380, GLUT_BITMAP_8_BY_13, "F2(Red): %s", m_lights[1].active ? "on" : "off");

	setColor(m_lights[2].active ? green : red);
	drawBitmapString_WithVars(-1, -140, -380, GLUT_BITMAP_8_BY_13, "F3(Green): %s", m_lights[2].active ? "on" : "off");

	setColor(m_lights[3].active ? green : red);
	drawBitmapString_WithVars(-1, -5, -380, GLUT_BITMAP_8_BY_13, "F4(Blue): %s", m_lights[3].active ? "on" : "off");

	setColor(m_running ? green : red);
	drawBitmapString_WithVars(-1, 130, -380, GLUT_BITMAP_8_BY_13, "F5(Animation): %s", m_running ? "on" : "off");

	setColor(blue);
	drawBitmapString(-1, 290, -380, GLUT_BITMAP_8_BY_13, "F6(Restart)");

	// light range
	drawBitmapString_WithVars(1, WINDOW_RIGHT - 30, WINDOW_TOP - 30, GLUT_BITMAP_8_BY_13, "Light range: %0.0f", m_lightRange);
	drawBitmapString_WithVars(1, WINDOW_RIGHT - 30, WINDOW_TOP - 50, GLUT_BITMAP_8_BY_13, "(+/- to change)", m_lightRange);

	setColor(m_unlitIsBlack ? red : green);
	drawBitmapString_WithVars(1, WINDOW_RIGHT - 30, WINDOW_TOP - 90, GLUT_BITMAP_8_BY_13, "Unlit color: %s", m_unlitIsBlack ? "Black" : "Gray");
	drawBitmapString_WithVars(1, WINDOW_RIGHT - 30, WINDOW_TOP - 110, GLUT_BITMAP_8_BY_13, "(Spacebar to change)", m_lightRange);
}

void drawTexts()
{
	drawTitleTexts();
	drawStatusTexts();
}

// Draws Statics, Game Objects, and Texts; in that order.
void draw()
{
	drawBackgrounds(); // game area background is drawn before so game objects are visible.
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
void onKeyDown(const unsigned char key, const int x, const int y)
{
	switch (key)
	{
	case 27:
		exit(0); // Exit when ESC is pressed
		break;

	case '+':
		m_lightRange += 50;
		break;

	case '-':
		m_lightRange = clampAbove0(m_lightRange - 50);
		break;

	case ' ':
		m_unlitIsBlack = !m_unlitIsBlack;
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
	glutTimerFunc(TIMER_PERIOD, onTimerTick_Game, 0);

	// Calculate the actual deltaTime
	int now = NOW;
	int deltaTime = now - m_lastTimerTickTime;
	m_lastTimerTickTime = now;

	if (m_running)
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
	case GLUT_KEY_F1:
		m_lights[0].active = !m_lights[0].active;
		break;

	case GLUT_KEY_F2:
		m_lights[1].active = !m_lights[1].active;
		break;

	case GLUT_KEY_F3:
		m_lights[2].active = !m_lights[2].active;
		break;

	case GLUT_KEY_F4:
		m_lights[3].active = !m_lights[3].active;
		break;

	case GLUT_KEY_F5:
		m_running = !m_running;
		break;

	case GLUT_KEY_F6:
		initNewSession();
	}

	// to refresh the window it calls display() function
	glutPostRedisplay();
}





//
// Main
//

int main(int argc, char *argv[])
{
	srand(time(0));

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(TOTAL_WIDTH, TOTAL_HEIGHT);
	glutCreateWindow("CTIS 164 Homework #4  |  S. Tarýk Çetin  |  Lighting Simulation");

	glutDisplayFunc(display);
	glutReshapeFunc(onResize);

	// Keyboard input
	glutKeyboardFunc(onKeyDown);
	glutSpecialUpFunc(onSpecialKeyDown);

	// prepare game
	initNewSession();
	m_running = true;

	// Smoothing shapes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Schedule the initial timer tick
	glutTimerFunc(TIMER_PERIOD, onTimerTick_Game, 0);

	glutMainLoop();
	
	return 0;
}
