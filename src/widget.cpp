#include "widget.h"
#include <OpenGL/glu.h>
#include <cfloat>
#include <netcdf.h>

#include <math.h>
#include <iostream>

#include <cmath>
#include <stdlib.h> 
#include <algorithm>

#include "advect.h"

#include <Eigen/Dense>

#include <QGLShaderProgram>
#include <QPixmap>

using namespace Eigen;

#include <fstream>
#include <iostream>
#include <ctime>


CGLWidget::CGLWidget(const QGLFormat& fmt) : 
QGLWidget(fmt),
fovy(30.f), znear(0.1f), zfar(10.f),
eye(0, 0, 2.5), center(0, 0, 0), up(0, 1, 0)
{
}

CGLWidget::~CGLWidget()
{
}

void CGLWidget::init(std::string view_filename){

     // reset trace xy if view filename exists
    // set trackball rotation, trackball scale, d->px, d->py, seed_offset_x, seed_offset_y

    trackball.init();

    std::string line;
    
    std::ifstream myfile (view_filename.c_str());
    int ctr = 0;
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            vvals[ctr++] = std::stod(line);
            // std::cout << vvals[ctr-1]<< '\n';
        }
      myfile.close();

      d->px = vvals[0];
      d->py = vvals[1];
      seed_offset_x = vvals[2];
      seed_offset_y = vvals[3];
      
      d->trace_xy.clear();
      d->trace_xy.push_back(d->px); d->trace_xy.push_back(d->py);

  }
  else {fprintf(stderr, "Unable to open view file. Showing default view.\n"); }

  

}

void CGLWidget::initializeGL()
{
    glewInit();

    // trackball.init();

    sphere = gluNewQuadric();
    gluQuadricDrawStyle(sphere, GLU_FILL);
    gluQuadricTexture(sphere, TRUE);
    gluQuadricNormals(sphere, GLU_SMOOTH);
    
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    
    glEnable(GL_DEPTH_TEST);
     // glEnable(GL_CULL_FACE);
     // glEnable(GL_TEXTURE_2D);

    // const unsigned char* glVer = glGetString(GL_VERSION);
    // std::cout << glVer << endl; 



    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);


    seed_offset_x = d->px/d->nu;
    seed_offset_y = d->py/d->nv;

    // trackball.setScale(vvals[4]);
    // // generate lic 
    // generate_seeds(d->px, d->py);
    // generate_lic();
    // load_texture();
    // trackball.setScale(vvals[5]);
    // licEnabled = true;

    

    CHECK_GLERROR();
}

void CGLWidget::resizeGL(int w, int h)
{
    trackball.reshape(w, h);
    glViewport(0, 0, w, h);

    CHECK_GLERROR();
}

double mag(double *x){
    return std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
}


// https://git.merproject.org/mer-core/qtbase/commit/595ed595eabe01a1cf11c8b846fd777de8233721
// https://forum.qt.io/topic/69315/qt5-7-qvector3d-unproject-mouse-picking/7
// https://dondi.lmu.build/share/cg/unproject-explained.pdf
QVector3D unproject(const QVector3D & thisvec, const QMatrix4x4 &modelView, const QMatrix4x4 &projection, const QRect &viewport)
{
    QMatrix4x4 inverse = QMatrix4x4( projection * modelView ).inverted();

    QVector4D tmp(thisvec, 1.0f);
    tmp.setX((tmp.x() - float(viewport.x())) / float(viewport.width()));
    tmp.setY((tmp.y() - float(viewport.y())) / float(viewport.height()));
    tmp = tmp * 2.0f - QVector4D(1.0f, 1.0f, 1.0f, 1.0f);

    QVector4D obj = inverse * tmp;
    if (qFuzzyIsNull(obj.w()))
        obj.setW(1.0f);
    obj /= obj.w();
    return obj.toVector3D();
}


void CGLWidget::set_lic_size(double lic_size, int lic_nmax, float step, double rate){

    this->lic_nmax = lic_nmax;
    this->lic_size = lic_size;
    this->step = step;
    this->rate = rate;

    
}



void CGLWidget::generate_seeds(double px, double py){
    double scale = trackball.getScale();
  
    double step = rate/scale;
    this->lsize = 0;

    for (double i=px-lic_size/scale; i<px+lic_size/scale-10e-12; i+=step){
        for (double j=py-lic_size/scale; j<py+lic_size/scale-10e-12; j+=step){
            lic_x.push_back(i);
            lic_y.push_back(j);
        }
        this->lsize++;
    }
    scx = px-lic_size/scale;
    scy = py-lic_size/scale;
    lic_res = step;
    lic_vals.resize(lic_x.size());
    // fprintf(stderr, "lic_x %ld %d\n", lic_x.size(), lsize);
    // fprintf(stderr, "Checking assertion lic_x %ld %d\n", lic_x.size(), lsize);

    
    assert(lsize*lsize==lic_x.size());

    /* Generate noise image */
    srand(5);
    noise.resize(lsize*lsize);
     for (int i = 0; i < lsize; i++){
        for (int j = 0; j< lsize; j++){
            noise[i*lsize + j] = float((rand() % 255))/255;
            // fprintf(stderr, "noise %f \n", noise[i*lsize + j]);

        }
    }

     // fprintf(stderr, "top left: (%f %f), bottom right: (%f %f)\n", px-lic_size/scale, py-lic_size/scale, px-lic_size/scale + step*lsize, py-lic_size/scale + step*lsize);

}

void CGLWidget::generate_lic(){

    double p[2];
    double nx[2];
    double lic_val;
    double  idx_x, idx_y;
    int ind_i, ind_j;
    double maxval = 0;

    for (size_t i=0; i<lic_x.size(); i++){

        lic_val = 0;
        p[0] = lic_x[i];
        p[1] = lic_y[i];
        // forward advect
        for (int j=0; j<lic_nmax; j++){
            if(advect(*d, p, nx, step)){


                p[0] = nx[0];
                p[1] = nx[1];

                // get index on noise field
                idx_x = (p[0]-scx)/lic_res;
                idx_y = (p[1]-scy)/lic_res;

                ind_i = floor(idx_x);
                ind_j = floor(idx_y);


                // get noise contribution from clamped positions
                if (idx_x > 0 && idx_y >0 && idx_x < lsize && idx_y <lsize)
                    lic_val += noise[ind_i * lsize  + ind_j] * (lic_nmax-j);
                else
                    break;

            }else{
                break;
            }

        }
        // exit(0);
        p[0] = lic_x[i];
        p[1] = lic_y[i];
        // backward advect
        for (int j=1; j<lic_nmax; j++){
            if(advect(*d, p, nx, -step)){

                p[0] = nx[0];
                p[1] = nx[1];

                 // get index on noise field
                idx_x = (p[0]-scx)/lic_res;
                idx_y = (p[1]-scy)/lic_res;

                ind_i = floor(idx_x);
                ind_j = floor(idx_y);

                // get noise contribution from clamped positions
                if (idx_x > 0 && idx_y >0 && idx_x < lsize && idx_y <lsize)
                    lic_val += noise[ind_i * lsize  + ind_j] * (lic_nmax-j);
                else
                    break;

            }else{
                break;
            }
        }

        lic_vals[i] = lic_val;
        if (maxval < lic_val)
            maxval = lic_val;

        

    }

    
    for (size_t i=0; i<lic_vals.size(); i++){

        lic_vals[i] /= maxval;
        // fprintf(stderr, "licvals %d %f\n", i, lic_vals[i]);
    }

}

void CGLWidget::paintGL()
{


    glClearColor(1, 1, 1, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
        
    projmatrix.setToIdentity();
    // projmatrix.perspective(fovy, (float)width()/height(), znear, zfar);
    // projmatrix.ortho(-0.5, 0.5, -0.5, 0.5, znear, zfar);
    projmatrix.ortho(-0.5, 0.5, -float(height())/width()/2.0, float(height())/width()/2.0, znear, zfar);
    mvmatrix.setToIdentity();
    mvmatrix.lookAt(eye, center, up);
    mvmatrix.rotate(trackball.getRotation());
    mvmatrix.scale(trackball.getScale());

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glLoadMatrixd(projmatrix.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixd(mvmatrix.data());




    glColor3f(0, 0, 0);
    glPointSize(1.0);


                                                                                                                                                                                                                                  

    float x, y;


    /* Texture */
    if (licEnabled == true){
        float scale = trackball.getScale();
        glPushMatrix();
      // glColor4f(1, 1, 1, 1);
        // glScalef(scale, scale, scale);
        glRotatef(-90, 0, 0, 1);
      // glTranslatef(-0.5f, -0.5f, 0.f);

        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);



        glVertex2f(-xval, -yval); glTexCoord2f(0, 0);
        glVertex2f(xval, -yval); glTexCoord2f(1, 0);
        glVertex2f(xval, yval); glTexCoord2f(1, 1);
        glVertex2f(-xval, yval); glTexCoord2f(0, 1);



        glEnd();
        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }

    /* Texture ends */

    /* drawing vector field */

    glColor3f(0, 0, 0);
    glPointSize(1.0);

   
    float alpha = 0.001;
    float vx, vy;
    glLineWidth(1.0);
    for (size_t i=0; i<d->nu; i+=10){
        for (size_t j=0; j<d->nv; j+=10){

            
            vx = d->u[j*d->nu+i];
            vy = d->v[j*d->nu+i];
            x = float(i)/d->nu-seed_offset_x;
            y = float(j)/d->nu-seed_offset_y;

            glBegin(GL_LINES);
            glVertex3f(x, y, 0);
            // fprintf(stderr, "%f %f \n", x, y);
            glVertex3f(x + alpha*vx, y + alpha*vy, 0.0);
            glEnd();

            glBegin(GL_POINTS);
            glVertex3f(x,y,0.0);
            glEnd();

        }
    }

   
    /* drawing streamline  */
    float x0, y0, x1, y1;
    x0 = d->trace_xy[0]/d->nu-seed_offset_x;
    y0 = d->trace_xy[1]/d->nv-seed_offset_y;

    glColor3f(0, 0, 1.0);
    glPointSize(8.0);
    glBegin(GL_POINTS);
    glVertex3f(x0,y0,0.0);
    glEnd();

    glColor3f(1, 0, 0);
    glLineWidth(2.0);
    for (size_t i=0; i<d->trace_xy.size()-2; i+=2){

        x0 = d->trace_xy[i]/d->nu-seed_offset_x;
        y0 = d->trace_xy[i+1]/d->nv-seed_offset_y;
        x1 = d->trace_xy[i+2]/d->nu-seed_offset_x;
        y1 = d->trace_xy[i+3]/d->nv-seed_offset_y;

         glBegin(GL_LINES);
         glVertex3f(x0, y0, 0.0);
         glVertex3f(x1, y1, 0.0);
         glEnd();
    }

    /* drawing interactive streamline  */

    // if (d->itrace_xy.size()>0){
    //     x0 = d->itrace_xy[0]/d->nu-seed_offset_x;
    //     y0 = d->itrace_xy[1]/d->nv-seed_offset_y;

    //     glColor3f(0, 0.5, 0);
    //     glPointSize(8.0);
    //     glBegin(GL_POINTS);
    //     glVertex3f(x0,y0,0.001);
    //     glEnd();

    //     glColor3f(1, 0, 0);
    //     glLineWidth(2.0);
    //     for (size_t i=0; i<d->itrace_xy.size()-2; i+=2){

    //         x0 = d->itrace_xy[i]/d->nu-seed_offset_x;
    //         y0 = d->itrace_xy[i+1]/d->nv-seed_offset_y;
    //         x1 = d->itrace_xy[i+2]/d->nu-seed_offset_x;
    //         y1 = d->itrace_xy[i+3]/d->nv-seed_offset_y;

    //          glBegin(GL_LINES);
    //          glVertex3f(x0, y0, 0.001);
    //          glVertex3f(x1, y1, 0.001);
    //          glEnd();
    //     }
    // }

    if (d->itraces_xy.size()>0){

        for (size_t h=0; h<d->itraces_xy.size(); h++){
            x0 = d->itraces_xy[h][0]/d->nu-seed_offset_x;
            y0 = d->itraces_xy[h][1]/d->nv-seed_offset_y;

            glColor3f(0, 0.5, 0);
            glPointSize(8.0);
            glBegin(GL_POINTS);
            glVertex3f(x0,y0,0.0);
            glEnd();

            glColor3f(1, 0, 0);
            glLineWidth(2.0);
            for (size_t i=0; i<d->itraces_xy[h].size()-2; i+=2){

                x0 = d->itraces_xy[h][i]/d->nu-seed_offset_x;
                y0 = d->itraces_xy[h][i+1]/d->nv-seed_offset_y;
                x1 = d->itraces_xy[h][i+2]/d->nu-seed_offset_x;
                y1 = d->itraces_xy[h][i+3]/d->nv-seed_offset_y;

                glBegin(GL_LINES);
                glVertex3f(x0, y0, 0.0);
                glVertex3f(x1, y1, 0.0);
                glEnd();
            }
        }
    }

    CHECK_GLERROR();
}

void CGLWidget::compute_interavtive_streamline(double sx, double sy, int direction){

     // d->itrace_xy.clear();

     std::vector<double> itrace_xy;
     double p[2] = {sx, sy};
     itrace_xy.push_back(p[0]); itrace_xy.push_back(p[1]);
    // advect
    for (int i=0; i<d->nmax; i++){

        double nx[2];
        if (advect(*d, p, nx, direction*step)){
            p[0] = nx[0]; p[1] = nx[1];
            itrace_xy.push_back(p[0]); itrace_xy.push_back(p[1]);
        }else{
            break;
        }

    }

    d->itraces_xy.push_back(std::move(itrace_xy));

}




void CGLWidget::mousePressEvent(QMouseEvent* e)
{
    // trackball.mouse_rotate(e->x(), e->y());


    // projmatrix.setToIdentity();
    // projmatrix.perspective(fovy, (float)width()/height(), znear, zfar);
    // mvmatrix.setToIdentity();
    // mvmatrix.lookAt(eye, center, up);
    // mvmatrix.rotate(trackball.getRotation());
    // mvmatrix.scale(trackball.getScale());


     double wx  = (e->x() - width()/double(2))/width()/trackball.getScale(), wy = (height()/double(2) - e->y())/height()/trackball.getScale();

        // double nsx = wx*(d->nu) + d->px , nsy = wy*(d->nv)+ d->py;
        double nsx = wx*(d->nu) + seed_offset_x*d->nu , nsy = wy*(d->nv)+ seed_offset_y*d->nv;


     if(e->modifiers() && Qt::ShiftModifier){
          double wx  = (e->x() - width()/double(2))/width()/trackball.getScale(); // screen norm offset scaled
          double wy =  (height()/double(2) - e->y())/height()/trackball.getScale();


        seed_offset_x = seed_offset_x + wx; // obj norm offset
        seed_offset_y = seed_offset_y + wy;
        d->px = nsx; d->py = nsy;

        // clear and update the trace_xy
        d->trace_xy.clear();
        d->trace_xy.push_back(d->px);
        d->trace_xy.push_back(d->py);

        licEnabled = false;

    }else{

        // glMatrixMode(GL_PROJECTION);
        // glLoadIdentity();
        // glLoadMatrixd(projmatrix.data());
        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();
        // glLoadMatrixd(mvmatrix.data());

        // // Import the viewPort
        //   GLint *params = new GLint(4);
        //   glGetIntegerv(GL_VIEWPORT, params);
        //   QRect vp = QRect(*params, *(params + 1), *(params + 2), *(params + 3));  


        fprintf(stderr, "pos %f %f, %f %f\n", wx, wy, nsx, nsy);                                                                                                                                                                                                                                               

        // double x, y;
        // // double seed_offset_x = d->px/d->nu, seed_offset_y = d->py/d->nv;
        // QVector3D frnt(e->x(), e->y(), 0);
        // QVector3D frnt_pt = unproject(frnt, mvmatrix, projmatrix, vp);

        // QVector3D back(e->x(), e->y(), 1);
        // QVector3D back_pt = unproject(back, mvmatrix, projmatrix, vp);

        // Vector3d dir(back_pt.x() - frnt_pt.x(), back_pt.y() - frnt_pt.y(), back_pt.z() -frnt_pt.z());
        // double magratio = frnt_pt.z()/abs((frnt_pt.z() - back_pt.z()));
        // double mag = dir.norm();
        // dir.normalize();

        // Vector3d sd = magratio*mag*dir + Vector3d(frnt_pt.x(), frnt_pt.y(), frnt_pt.z());
       
        // double sx = (d->nu)*(sd(0)+seed_offset_x);
        // double sy = (d->nv)*(-sd(1)+seed_offset_y);


    

        compute_interavtive_streamline(nsx, nsy, 1);
        compute_interavtive_streamline(nsx, nsy, -1);

    }
    updateGL();

}

void CGLWidget::mouseMoveEvent(QMouseEvent* e)
{
    // trackball.motion_rotate(e->x(), e->y());
    updateGL();
}

void CGLWidget::wheelEvent(QWheelEvent* e)
{
    trackball.wheel(e->delta());
    fprintf(stderr, "scale %f\n", trackball.getScale());
    updateGL();
}

std::string get_timestamp()
{
    auto now = std::time(nullptr);
    char buf[sizeof("YYYY-MM-DD-HH:MM:SS")];
    return std::string(buf,buf + 
        std::strftime(buf,sizeof(buf),"%F-%T",std::localtime(&now)));
}


void CGLWidget::keyPressEvent(QKeyEvent *ev)
{   

    lic_x.clear();
    lic_y.clear();

    if (ev->text().toStdString().c_str()[0]=='s'){

        xval = lic_size/d->nu/trackball.getScale();
        yval = lic_size/d->nv/trackball.getScale();

        licScale = trackball.getScale(); // will be stored in file if view saved
        generate_seeds(d->px, d->py);
        generate_lic();
        load_texture();
        updateGL();
        licEnabled = true;

        
    }

    if (ev->text().toStdString().c_str()[0]=='c'){

        d->itraces_xy.clear();
        updateGL();
    }

    // save view
    if (ev->text().toStdString().c_str()[0]=='p'){

        // for (int i=0;i<16; i++)
        //     fprintf(stderr, "%f ", projmatrix.data()[i]);
        // fprintf(stderr, "\n");
        std::string fname = get_timestamp()+".txt";
        fprintf(stderr, "Saving view into %s \n", fname.c_str());
        std::ofstream f1 (fname);
          if (f1.is_open())
          {
            // for (int i=0;i<16; i++)
            //     f1 << projmatrix.data()[i]<< "\n";
            // for (int i=0;i<16; i++)
            //     f1 << mvmatrix.data()[i]<< "\n";

            f1 << d->px << "\n";
            f1 << d->py << "\n";
            f1 << seed_offset_x << "\n";
            f1 << seed_offset_y << "\n";
            f1 << licScale << "\n";
            f1 << trackball.getScale() << "\n";
            // f1 << trackball.getRotation() << "\n";
            f1.close();
          }
            
    }

    // save view
    if (ev->text().toStdString().c_str()[0]=='i'){

        QImage img = grabFrameBuffer();
        std::string fname = get_timestamp()+".png";
        fprintf(stderr, "Saving screenshot into %s \n", fname.c_str());
        img.save(fname.c_str());

    }


}



int ctr=0;
void CGLWidget::load_texture(){

    value = 1/trackball.getScale();
    glBindTexture(GL_TEXTURE_2D, tex);

    // Black/white checkerboard
    // float pixels[] = {
    //     0.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    //     0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f
    // };

    // float pixels[] = {
    //     0.0f, 1.0f,
    //     1.0f, 0.0f
    // };


    float pixels[lsize*lsize];
    for (size_t i=0; i<lsize; i++){
        for (int j=0; j<lsize; j++){
            pixels[j*lsize + i ] = float(lic_vals[i*lsize + j]);
        }
    }


    // glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 2, 2, 0, GL_LUMINANCE, GL_FLOAT, pixels);
      // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lsize, lzise, 0, GL_RGB, GL_FLOAT, rgb.data());

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, lsize, lsize, 0, GL_LUMINANCE, GL_FLOAT, pixels);

   
  
    CHECK_GLERROR();

}
