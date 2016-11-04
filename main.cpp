#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

GLFWwindow* window = NULL;
int gl_width = 360;
int gl_height = 240;
float gl_aspect_ratio = (float)gl_width/gl_height;

#include "init_gl.h"
#include "maths_funcs.h"
#include "Shader.h"
#include "load_obj.h"
#include "FlyCam.h"
#include "GJK.h"

int main() {
	if (!init_gl(window, gl_width, gl_height)){ return 1; }
	float* vp = NULL;
	int point_count = 0;
	load_obj("cube.obj", &vp, &point_count);

	//Collision mesh for GJK
	float box_points[] = {
		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		-0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f
	};

	GLuint vao;
	GLuint points_vbo;
	glGenBuffers(1, &points_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
	glBufferData(GL_ARRAY_BUFFER, point_count*3*sizeof(float), vp, GL_STATIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	free(vp);

	//Load shader
	Shader box_shader("MVP.vert", "uniform_colour.frag");
	GLuint colour_loc = glGetUniformLocation(box_shader.prog_id, "colour");
	glUseProgram(box_shader.prog_id);

	mat4 box_M[5];
	box_M[0] = translate(identity_mat4(), vec3(0.5f, 0.5f, 0.5f));//translate(rotate_y_deg(identity_mat4(), 45), vec3(-1.5f, 0, -1.5f));
	box_M[1] = translate(identity_mat4(), vec3(-1.5f, 0, 1.5f));
	box_M[2] = translate(identity_mat4(), vec3(0, 0, 0));
	box_M[3] = translate(identity_mat4(), vec3(1.5f, 0, -1.5f));
	box_M[4] = translate(identity_mat4(), vec3(1.5f, 0, 1.5f));
	vec4 box_colour[5];

	vec3 player_pos = vec3(0.5f-0.58f, 0.5f, 0.5f+0.86f);
	mat4 player_M = translate(identity_mat4(), player_pos);
	vec4 player_colour = vec4(0.1f, 0.8f, 0.3f, 1.0f);
	float player_speed = 10;

	Collider box_collider[5];
	box_collider[0].pos = vec3(0.5f, 0.5f, 0.5f);//vec3(-1.5f, 0, -1.5f);
	box_collider[1].pos = vec3(-1.5f, 0, 1.5f);
	box_collider[2].pos = vec3(0, 0, 0);
	box_collider[3].pos = vec3(1.5f, 0, -1.5f);
	box_collider[4].pos = vec3(1.5f, 0, 1.5f);
	for(int i=0; i<5; i++){
		box_collider[i].points = box_points;
		box_collider[i].num_points = 8;
		box_collider[i].M = box_M[i];
		box_collider[i].M_inverse = inverse(box_M[i]);
		box_colour[i] = vec4(0.8f, 0.1f, 0.1f, 1);
	}

	Collider player_collider;
	player_collider.pos = player_pos;
	player_collider.points = box_points;
	player_collider.num_points = 8;
	player_collider.M = identity_mat4();
	player_collider.M_inverse = identity_mat4();

	//Camera setup
	FlyCam fly_cam;
	fly_cam.init(vec3(2,3,6), vec3(0,0,0));

	glUniformMatrix4fv(box_shader.V_loc, 1, GL_FALSE, fly_cam.V.m);
	glUniformMatrix4fv(box_shader.P_loc, 1, GL_FALSE, fly_cam.P.m);

	double curr_time = glfwGetTime(), prev_time, dt;
	//-------------------------------------------------------------------------------------//
	//-------------------------------------MAIN LOOP---------------------------------------//
	//-------------------------------------------------------------------------------------//
	while (!glfwWindowShouldClose(window)) {
		//Get dt
		prev_time = curr_time;
		curr_time = glfwGetTime();
		dt = curr_time - prev_time;
		if (dt > 0.1) dt = 0.1;

		// static float fps_timer = 0.0f;
		// fps_timer+=dt;
		// if(fps_timer>0.2f){
		// 	char title_string[64];
		// 	sprintf(title_string, "GJK - %.2fHz", 1/dt);
		// 	glfwSetWindowTitle(window, title_string);
		// 	fps_timer = 0.0f;
		// }

		//Get Input
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window, GL_TRUE);
			continue;
		}
		static bool fly_cam_enabled = false;
		static bool F_was_pressed = false;
		if (glfwGetKey(window, GLFW_KEY_F)) {
			if(!F_was_pressed) {
				fly_cam_enabled = !fly_cam_enabled;
				F_was_pressed = true;
			}
		}
		else F_was_pressed = false;
		if(fly_cam_enabled) fly_cam.update(dt);
		else{
			//Move player
			if(g_input[MOVE_FORWARD]) {
				vec3 xz_proj = normalise(vec3(fly_cam.fwd.v[0], 0, fly_cam.fwd.v[2]));
				player_pos += xz_proj*player_speed*dt;
			}
			if(g_input[MOVE_LEFT]) {
				vec3 xz_proj = normalise(vec3(fly_cam.rgt.v[0], 0, fly_cam.rgt.v[2]));
				player_pos -= xz_proj*player_speed*dt;
			}
			if(g_input[MOVE_BACK]) {
				vec3 xz_proj = normalise(vec3(fly_cam.fwd.v[0], 0, fly_cam.fwd.v[2]));
				player_pos -= xz_proj*player_speed*dt;			
			}
			if(g_input[MOVE_RIGHT]) {
				vec3 xz_proj = normalise(vec3(fly_cam.rgt.v[0], 0, fly_cam.rgt.v[2]));
				player_pos += xz_proj*player_speed*dt;			
			}
			player_M = translate(identity_mat4(), player_pos);
		}

		//if (glfwGetKey(window, GLFW_KEY_G)) {
			player_collider.pos = player_pos;
			for(int i=0; i<1; i++){
				vec3 mtv(0,0,0); //minimum translation vector
				if(gjk(player_collider, box_collider[i], mtv)) 
					box_colour[i] = vec4(0.8f, 0.7f, 0.0f, 1);
				else 
					box_colour[i] = vec4(0.8f, 0.1f, 0.1f, 1);
				
				player_pos += mtv;
				player_M = translate(identity_mat4(), player_pos);
			}
		//}
		//Rendering
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(box_shader.prog_id);
		glBindVertexArray(vao);
		glUniformMatrix4fv(box_shader.V_loc, 1, GL_FALSE, fly_cam.V.m);
		for(int i=0; i<1; i++){
			glUniform4fv(colour_loc, 1, box_colour[i].v);
			glUniformMatrix4fv(box_shader.M_loc, 1, GL_FALSE, box_M[i].m);
			glDrawArrays(GL_TRIANGLES, 0, point_count);
		}

		glUniformMatrix4fv(box_shader.M_loc, 1, GL_FALSE, player_M.m);
		glUniform4fv(colour_loc, 1, player_colour.v);
		glDrawArrays(GL_TRIANGLES, 0, point_count);

		glfwSwapBuffers(window);
	}//end main loop
	return 0;
}
