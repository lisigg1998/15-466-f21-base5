#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void reset_level();
	void move_donut();
	void move_ghost();

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// sfx sample
	std::shared_ptr< Sound::PlayingSample > ghost_sfx; // game begin

	const float DONUT_PICKUP_RADIUS = 0.2f;

	int score = 0;
	WalkPoint donut_at;
	WalkPoint ghost_at;
	Scene::Transform *donut_transform = nullptr;
	Scene::Transform *ghost_transform = nullptr;
	bool game_over = false;
	float timer = 5.0f;

	std::mt19937 mt;

	//player info:
	struct Player {
		WalkPoint at;
		float rotation = 0.0f;
		glm::vec3 velocity = glm::vec3(0.0f);
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;
};
