#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <time.h>       /* time */
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

GLuint phonebank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("track.pnct"));
	phonebank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > phonebank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("track.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = phonebank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = phonebank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > ghost_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("ghost.wav"));
	});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("track.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

PlayMode::PlayMode() : scene(*phonebank_scene) {

	for (auto &transform : scene.transforms) {
		std::cout << transform.name << std::endl;
		std::cout << "transform rotation: " << transform.rotation.w << " " <<  transform.rotation.x << " " <<transform.rotation.y << " " <<transform.rotation.z << " " << std::endl;

		if (transform.name == "Donut")
			donut_transform = &transform;
		if( transform.name == "Player"){
			player.transform = &transform;
		}
	}
	assert(donut_transform != nullptr);
	assert(player.transform != nullptr);

	// create a ghost transform
	scene.transforms.emplace_back();
	ghost_transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;
	player.camera->transform->position = glm::vec3(0.0f,0.0f, 50.0f);


	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);
	move_donut();
	move_ghost();


}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			reset_level();
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	}

	return false;
}

void PlayMode::move_donut() {
	glm::vec3 new_donut_pos = glm::vec3((mt() / float(mt.max())) * 6.0f - 3.0f, 0.0f, (mt() / float(mt.max())) * 6.0f - 3.0f);
	donut_at = walkmesh->nearest_walk_point(new_donut_pos);
	donut_transform->position = walkmesh->to_world_point(donut_at);
}

void PlayMode::move_ghost() {
	glm::vec3 new_enemy_pos = glm::vec3((mt() / float(mt.max())) * 6.0f - 3.0f, 0.0f, (mt() / float(mt.max())) * 6.0f - 3.0f);
	ghost_at = walkmesh->nearest_walk_point(new_enemy_pos);
	// make sure ghost is not randomized to player's location
	while(glm::length(walkmesh->to_world_point(ghost_at) - player.transform->position) <= 2* DONUT_PICKUP_RADIUS) {
		glm::vec3 new_enemy_pos = glm::vec3((mt() / float(mt.max())) * 6.0f - 3.0f, 0.0f, (mt() / float(mt.max())) * 6.0f - 3.0f);
		ghost_at = walkmesh->nearest_walk_point(new_enemy_pos);
	}
	ghost_transform->position = walkmesh->to_world_point(ghost_at);
}

void PlayMode::reset_level() {
	game_over = false;
	score = 0;

	move_donut();
	move_ghost();

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);
}

void PlayMode::update(float elapsed) {
	if (game_over == true) {
		return;
	}

	// update ghost position randomly
	timer -= elapsed;
	if (timer < 0) {
		timer = (mt() / float(mt.max())) * 5.0f + 5.0f;
		move_ghost();
	}

	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 0.2f;
		constexpr float PlayerMinSpeed = 0.001f;
		glm::vec2 move = glm::vec2(0.0f);
		if (!game_over) {
			if (left.pressed && !right.pressed) {
				player.velocity.x = -PlayerSpeed;
			}
			if (!left.pressed && right.pressed) {
				player.velocity.x = PlayerSpeed;
			}
			if (down.pressed && !up.pressed) {
				player.velocity.y = -PlayerSpeed;
			}
			if (!down.pressed && up.pressed) {
				player.velocity.y = PlayerSpeed;
			}

			float speed = glm::length(player.velocity);
			if (speed > PlayerSpeed) player.velocity = player.velocity * PlayerSpeed / speed;
			if (!left.pressed && !right.pressed && !up.pressed && !down.pressed) {
				if (speed >= PlayerMinSpeed) player.velocity *= 0.9f;
				else player.velocity = glm::vec3(0.0f);
			}
			
		}

		//make it so that moving diagonally doesn't go faster:
		//if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(player.velocity.x, player.velocity.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		// alarm sound when player is near a ghost
		if (glm::length(walkmesh->to_world_point(ghost_at) - player.transform->position) <= 7* DONUT_PICKUP_RADIUS) {
			if( ghost_sfx == nullptr || ghost_sfx->stopped )ghost_sfx = Sound::loop_3D(*ghost_sample, 1.0f, ghost_transform->position, 0.05f);		
		}
		else {
			if(ghost_sfx != nullptr ) ghost_sfx->stop();
		}

		// catch the donut
		if (glm::length(player.transform->position - donut_transform->position) <= DONUT_PICKUP_RADIUS) {
			score++;
			move_donut();
		}
		// caught by ghost
		if (glm::length(walkmesh->to_world_point(ghost_at) - player.transform->position) <= DONUT_PICKUP_RADIUS) {
			if(ghost_sfx != nullptr ) ghost_sfx->stop();
			game_over = true;
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = player.transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 at = frame[3];
		Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		std::string game_text = " Score: " + std::to_string(score);
		if (game_over) game_text = "Game Over. R to restart";

		constexpr float H = 0.09f;
		lines.draw_text(game_text,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(game_text,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
