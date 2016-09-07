/*
 * spacer.cpp
 *  
 *
 *  Created by Simon Freedman
 *  Copyright 2013 University of Chicago. All rights reserved.
 *
 */

#include "spacer.h"
#include "filament_ensemble.h"
//spacer class

spacer::spacer( array<double, 3> pos, 
        double mlen, filament_ensemble * network, 
        array<int, 2> mystate, 
        array<int, 2> myfindex, 
        array<int, 2> mylindex,
        array<double, 2> myfov, 
        double delta_t, 
        double temp,
        double v0, 
        double stiffness, 
        double max_ext_ratio, 
        double ron, double roff, double rend, 
        double fstall, double fbreak, double engBind,
        double vis, string bc) {
    
    vs          = v0;
    mk          = stiffness;//rng(10,100); 
    
    stall_force   = fstall;
    break_force   = fbreak;
    max_bind_dist = sqrt(engBind/stiffness);
    var_bind_dist = (2.0/3.0)*engBind/stiffness;

    mld         = mlen;
    dt          = delta_t;
    kon         = ron*dt;
    koff        = roff*dt;
    kend        = rend*dt;
    mphi        = pos[2];
    temperature = temp;
    state       = mystate;
    f_index     = myfindex; //filament index for each head
    l_index     = mylindex; //link index for each head
    fov         = myfov;
    BC          = bc; 
    actin_network = network;
    damp=(4*pi*vis*mld);
    
    
    max_ext     = max_ext_ratio*mlen;
    eps_ext     = 0.01*max_ext;
    
    shear       = 0;
    tension     = 0;
    force       = {0,0}; // force on the spring  
    kinetic_energy = 0; //assume m = 1
    pos_a_end = {0, 0}; // pos_a_end = distance from pointy end -- by default 0
                        // i.e., if l_index[hd] = j, then pos_a_end[hd] is the distance to the "j+1"th actin
    
    array<double, 2> posH0 = boundary_check(0, pos[0]-0.5*mld*cos(mphi), pos[1]-0.5*mld*sin(mphi)); 
    array<double, 2> posH1 = boundary_check(1, pos[0]+0.5*mld*cos(mphi), pos[1]+0.5*mld*sin(mphi)); 
    hx[0] = posH0[0];
    hy[0] = posH0[1];
    hx[1] = posH1[0];
    hy[1] = posH1[1];
    
    disp = rij_bc(BC, hx[1]-hx[0], hy[1]-hy[0], fov[0], fov[1], actin_network->get_delrx()); 
    
    if (state[0] == 1){
        pos_a_end[0] = dist_bc(BC, actin_network->get_end(f_index[0], l_index[0])[0] - hx[0],
                actin_network->get_end(f_index[0], l_index[0])[1] - hy[0], fov[0], fov[1], 0);
    }
    if (state[1] == 1){
        pos_a_end[1] = dist_bc(BC, actin_network->get_end(f_index[1], l_index[1])[0] - hx[1],
                                   actin_network->get_end(f_index[1], l_index[1])[1] - hy[1], fov[0], fov[1], 0);
    }
    
    prv_rnd_x = {0,0};
    prv_rnd_y = {0,0};

}


spacer::spacer( array<double, 4> pos, 
        double mlen, filament_ensemble * network, 
        array<int, 2> mystate, 
        array<int, 2> myfindex, 
        array<int, 2> mylindex,
        array<double, 2> myfov, 
        double delta_t, 
        double temp,
        double v0, 
        double stiffness, 
        double max_ext_ratio, 
        double ron, double roff, double rend, 
        double fstall, double fbreak, double engBind,
        double vis, string bc) {
    
    vs          = v0;
    mk          = stiffness;
    
    stall_force = fstall;
    break_force = fbreak;
    max_bind_dist = sqrt(engBind/stiffness);
    var_bind_dist = (2.0/3.0)*engBind/stiffness;
    
    mld         = mlen;
    dt          = delta_t;
    kon         = ron*dt;
    koff        = roff*dt;
    kend        = rend*dt;
    mphi        = pos[2];
    temperature = temp;
    state       = mystate;
    f_index     = myfindex; //filament index for each head
    l_index     = mylindex; //link index for each head
    fov         = myfov;
    BC          = bc; 
    actin_network = network;
    damp=(4*pi*vis*mld);
    
    max_ext     = max_ext_ratio*mlen;
    eps_ext     = 0.01*max_ext;
    
    shear       = 0;
    tension     = 0;
    force       = {0,0}; // force on the spring  
    kinetic_energy = 0;
    pos_a_end = {0, 0}; // pos_a_end = distance from pointy end -- by default 0
                        // i.e., if l_index[hd] = j, then pos_a_end[hd] is the distance to the "j+1"th actin

    array<double, 2> posH0 = boundary_check(0, pos[0], pos[1]); 
    array<double, 2> posH1 = boundary_check(1, pos[0]+pos[2], pos[1]+pos[3]); 
    hx[0] = posH0[0];
    hy[0] = posH0[1];
    hx[1] = posH1[0];
    hy[1] = posH1[1];
    
    disp = rij_bc(BC, hx[1]-hx[0], hy[1]-hy[0], fov[0], fov[1], actin_network->get_delrx()); 

    if (state[0]){
        pos_a_end[0] = dist_bc(BC, actin_network->get_end(f_index[0], l_index[0])[0] - hx[0],
                                   actin_network->get_end(f_index[0], l_index[0])[1] - hy[0], fov[0], fov[1], 0);
    }
    if (state[1]){
        pos_a_end[1] = dist_bc(BC, actin_network->get_end(f_index[1], l_index[1])[0] - hx[1],
                                   actin_network->get_end(f_index[1], l_index[1])[1] - hy[1], fov[0], fov[1], 0);
    }
    
    prv_rnd_x = {0,0};
    prv_rnd_y = {0,0};

}


 spacer::~spacer(){};


void spacer::set_bending(double force_constant, double ang){
    kb  = force_constant;
    th0 = ang;
}


void spacer::update_force()
{ 
    update_bending(0);
    update_bending(1);
    update_angle(); // need to recalculate this, since heads might've moved

    tension = (mk)*(hypot(disp[0], disp[1]) - mld);
    force = {tension*cos(mphi), tension*sin(mphi)};
    
}


void spacer::update_bending(int hd)
{
    if (state[hd] != 1)
        return;
    
    double th, thdiff1, thdiff2, Cam1am1, Caa, Caam1, Crat1, Crat2, coef1, coef2;
    array<double, 2> ram1, ra, fAct, fHd;

    //cout<<"\nDEBUG: (hx["<<hd<<"],hy["<<hd<<"]) = ("<<hx[hd]<<" , "<<hy[hd]<<");";
    ram1 = rij_bc(BC, 
            hx[hd] - actin_network->get_end(f_index[hd], l_index[hd])[0],
            hy[hd] - actin_network->get_end(f_index[hd], l_index[hd])[1],
            fov[0], fov[1], actin_network->get_delrx());
    //ra = hd == 0 ? disp : {-(disp[0]), -(disp[1])};
    ra = {pow(-1, hd)*disp[0], pow(-1, hd)*disp[1]};

    Cam1am1 = ram1[0]*ram1[0] + ram1[1]*ram1[1];
    Caa     = ra[0]*ra[0]   + ra[1]*ra[1];
    Caam1   = ram1[0]*ra[0]   + ram1[1]*ra[1];

    //cout<<"\nDEBUG: (Ca-1a-1, Ca-1a, Caa) = ("<<Cam1am1<<" , "<<Caam1<<" , "<<Caa<<");";
    Crat1   = Caam1 / Caa;
    Crat2   = Caam1 / Cam1am1;
    //cout<<"\nDEBUG: (Crat1, Crat2) = "<<Crat1<<" , "<<Crat2<<";";

    th      = angBC(atan2(ra[1], ra[0]) - atan2(ram1[1], ram1[0]));
    thdiff1 = angBC( th - th0);
    thdiff2 = angBC(-th - th0);
  //  cout<<"\nDEBUG: (th, thdiff1, thdiff2) = ("<<th<<" , "<<thdiff1<<" , "<<thdiff2<<")";
    //cout<<"\nDEBUG: Cam1am1, Caam1, Caa = ( "<<Cam1am1<<" , "<<Caam1<<" , "<<Caa<<" )";
    
    coef1 = -kb*1.0/sqrt(Cam1am1*Caa);
    coef2 = coef1;

    if( fabs(th) > maxSmallAngle ){
        coef1 *= thdiff1 / sin( th);
        coef2 *= thdiff2 / sin(-th);
    }
    else{
        coef1 *= thdiff1 / sin(sgn( th)*maxSmallAngle);
        coef2 *= thdiff2 / sin(sgn(-th)*maxSmallAngle);
    }

    if( fabs(thdiff2) > maxSmallAngle )

//    cout<<"\nDEBUG: (kb, C1, C2) = ("<<kb<< " , "<<coef1<<" , "<<coef2<<")";
    
//    cout<<"\nDEBUG:coefs = ( "<<coef1<<" , "<<coef2<<" )";

    fHd  = {coef1 * (Crat1 * ra[0] - ram1[0]), coef1 * (Crat1 * ra[1] - ram1[1])};
    fAct = {coef2 * (ra[0] - Crat2 * ram1[0]), coef2 * (ra[1] - Crat2 * ram1[1])};
    cout<<"\nDEBUG: fHd =  ("<<fHd[0]<<" , "<<fHd[1]<<")"; 
    cout<<"\nDEBUG: fAct = ("<<fAct[0]<<" , "<<fAct[1]<<")"; 

    // Update actin bead and xlink bead
    actin_network->update_forces(f_index[hd], l_index[hd] + 1, fAct[0], fAct[1]);
    int h2 = pr(hd);
    if (state[h2] == 1)
        actin_update_hd(h2, fHd);
    else{
        array<double, 2> pos = boundary_check(h2, hx[h2] + fHd[0]*dt/damp, hy[h2] + fHd[1]*dt/damp);
        hx[h2] = pos[0];
        hy[h2] = pos[1];
    }

}

double spacer::get_kb(){
    return kb;
}

double spacer::get_th0(){
    return th0;
}

void spacer::identify(){
    cout<<"I am a spacer";
}