/*
 * motor.cpp
 *  
 *
 *  Created by Shiladitya Banerjee on 9/3/13.
 *  Copyright 2013 University of Chicago. All rights reserved.
 *
 */

#include "globals.h"
#include "motor.h"
#include "motor_ensemble.h"
#include "filament_ensemble.h"

//motor_ensemble class
template <class filament_ensemble_type>
motor_ensemble<filament_ensemble_type>::motor_ensemble(double mdensity, array<double, 2> myfov, double delta_t, double temp, 
        double mlen, filament_ensemble_type * network, double v0, double stiffness, double max_ext_ratio, double ron, double roff, double rend, 
        double actin_len, double vis, vector<array<double,3> > positions, string BC) {
    
    fov = myfov;
    mld =mlen;
    gamma = 0;
    tMove=0;//10;
    f_network=network;
    
    int nm = int(ceil(mdensity*fov[0]*fov[1]));
    cout<<"\nDEBUG: Number of motors:"<<nm<<"\n";

    double alpha = 1, motorx, motory, mang;
    array<double, 3> motor_pos; 
    for (int i=0; i< nm; i++) {
        
        if ((unsigned int)i < positions.size()){
            motorx = positions[i][0];
            motory = positions[i][1];
            mang   = positions[i][2];
        }else{
            motorx = rng(-0.5*(fov[0]*alpha-mld),0.5*(fov[0]*alpha-mld));
            motory = rng(-0.5*(fov[1]*alpha-mld),0.5*(fov[1]*alpha-mld));
            mang   = rng(0,2*pi);
        }
        motor_pos = {motorx, motory, mang};

        n_motors.push_back(new motor<filament_ensemble_type>( motor_pos, mld, f_network,{0, 0}, {-1,-1}, {-1,-1}, fov, delta_t, temp, 
                    v0, stiffness, max_ext_ratio, ron, roff, rend, actin_len, vis, BC));
        
    }
}

template <class filament_ensemble_type>
motor_ensemble<filament_ensemble_type>::motor_ensemble(vector<vector<double> > motors, array<double, 2> myfov, double delta_t, double temp, 
        double mlen, filament_ensemble_type * network, double v0, double stiffness, double max_ext_ratio, double ron, double roff, double rend, 
        double actin_len, double vis, string BC) {
    
    fov = myfov;
    mld = mlen;
    gamma = 0;
    tMove = 0;
    f_network=network;
    
    int nm = motors.size();
    cout<<"\nDEBUG: Number of motors:"<<nm<<"\n";

    array<double, 4> motor_pos;
    array<int, 2> f_index, l_index, state;

    for (int i=0; i< nm; i++) {
        
        motor_pos = {motors[i][0], motors[i][1], motors[i][2], motors[i][3]};
        
        f_index = {int(motors[i][4]), int(motors[i][5])};
        l_index = {int(motors[i][6]), int(motors[i][7])};

        state = {f_index[0] == -1 && l_index[0] == -1 ? 0 : 1, f_index[1] == -1 && l_index[1] == -1 ? 0 : 1};  

        n_motors.push_back(new motor<filament_ensemble_type>( motor_pos, mld, f_network, state, f_index, l_index, fov, delta_t, temp, 
                    v0, stiffness, max_ext_ratio, ron, roff, rend, actin_len, vis, BC));
    }
}

template <class filament_ensemble_type>
motor_ensemble<filament_ensemble_type>::~motor_ensemble( ){ 
    cout<<"DELETING MOTOR ENSEMBLE\n";
    int s = n_motors.size();
    for (int i = 0; i < s; i++){
        delete n_motors[i];
    }
    n_motors.clear();
};

template <class filament_ensemble_type>
int motor_ensemble<filament_ensemble_type>::get_nmotors( ){ 
    return n_motors.size();
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::kill_heads(int hd){
    for (unsigned int i = 0; i < n_motors.size(); i++)
        n_motors[i]->kill_head(hd);
}

//check if any motors attached to filaments that no longer exist; 
// if they do, detach them
// Worst case scenario, this is a O(m*n*p) function,
// where m is the number of filaments
//       n is the number of rods per filament
//       p is the number of motors
// However: we don't expect to fracture often, 
// so this loop should rarely if ever be accessed.
    

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::check_broken_filaments()
{
    vector<int> broken_filaments = f_network->get_broken();
    array<int, 2> f_index;
    
    for (unsigned int i = 0; i < broken_filaments.size(); i++){
        
        for(unsigned int j = 0; j < n_motors.size(); j++){
            
            f_index = n_motors[j]->get_f_index();

            if(f_index[0] == broken_filaments[i]){
                n_motors[j]->detach_head(0);
                //cout<<"\nDEBUG: detaching head 0 of motor "<<j<<" from broken filament "<<broken_filaments[i];
            }

            if(f_index[1] == broken_filaments[i]){
                n_motors[j]->detach_head(1);
                //cout<<"\nDEBUG: detaching head 1 of motor "<<j<<" from broken filament "<<broken_filaments[i];
            }
        }
    }


}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::motor_walk(double t)
{

    this->check_broken_filaments();
    int nmotors_sz = int(n_motors.size());
    #pragma omp parallel for
    
    for (int i=0; i<nmotors_sz; i++) {
       
//        cout<<"\nDEBUG: motor "<<i;
        array<int, 2> s = n_motors[i]->get_states();
        
        if (t >= tMove){
           
            if (s[0] == 1)         n_motors[i]->step_onehead(0);
            else if (s[0] == 0)    n_motors[i]->brownian_relax(0);
            if (s[1] == 1)         n_motors[i]->step_onehead(1);
            else if (s[1] == 0)    n_motors[i]->brownian_relax(1);
            
            n_motors[i]->update_angle();
            n_motors[i]->update_force();
            //n_motors[i]->update_force_fraenkel_fene();
            n_motors[i]->actin_update();
        }
        
        if (!s[0]){
            n_motors[i]->attach(0);
            //if(attached && s[1] == 0) n_motors[i]->relax_head(1);
        }
        if (!s[1]){
            n_motors[i]->attach(1);
            //if(attached && s[0] == 0) n_motors[i]->relax_head(0);
        }
    
    }
    
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::motor_write(ostream& fout)
{
    for (unsigned int i=0; i<n_motors.size(); i++) {
        fout<<n_motors[i]->write();
    } 
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::add_motor(motor<filament_ensemble_type> * m)
{
    n_motors.push_back(m);
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::set_shear(double g)
{
    for (unsigned int i=0; i<n_motors.size(); i++)
        n_motors[i]->set_shear(g);
    
    gamma = g;
}

template <class filament_ensemble_type> 
void motor_ensemble<filament_ensemble_type>::print_ensemble_thermo(){
    double KE=0, PEs=0;
    for (unsigned int m = 0; m < n_motors.size(); m++)
    {
        KE += n_motors[m]->get_kinetic_energy();
        PEs += n_motors[m]->get_stretching_energy();
        //PEs += n_motors[m]->get_stretching_energy_fene();
    }
    cout<<"\nAll Fs\t:\tKE = "<<KE<<"\tPEs = "<<PEs<<"\tPEb = "<<0<<"\tTE = "<<(KE+PEs);
}
template class motor_ensemble<ATfilament_ensemble>;
