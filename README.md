rayCast
=======

This is a GLSL implementation directly converted from the source/tutorial: 

http://www.daimi.au.dk/~trier/?page_id=98

It requires GLUT and GLEW, and has been built successufly on nVidia ( Linux ) and ATI ( MaxOS) graphic card. 

Built:
g++ main.cpp -L/usr/X11R6/lib -L/usr/lib64 -lGL -lGLU -lglut -lGLEW -lm -o rayCaster

Note:

It has odd boundary along the volume, however the original cg implementation doesn't have. It might be a problem in converting. 
