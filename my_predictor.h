// my_predictor.h
// This file contains a sample my_predictor class.
// It has a simple 32,768-entry gshare with a history length of 15 and a
// simple direct-mapped branch target buffer for indirect branch prediction.
#include <iostream>
using namespace std;

#define DEBUG 0
#define HISTORY 0
#define MAX_ITER 16
class my_update : public branch_update {
public:
	unsigned int index;
    int prediction_iter;
    int btb_miss_iter;

    my_update (void) : branch_update(), 
                        index(0), prediction_iter(MAX_ITER), btb_miss_iter(MAX_ITER) {
    }
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15

	my_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];
	unsigned int targets[1<<TABLE_BITS];
    unsigned int hashmap[MAX_ITER];
    unsigned int lfutable[1<<TABLE_BITS];

	my_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
		memset (targets, 0, sizeof (targets));
        memset (lfutable, MAX_ITER, sizeof (lfutable));
        init_hashmap();
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} /*else {
			//u.direction_prediction (true);
		}*/
		if (b.br_flags & BR_INDIRECT) {
			//u.target_prediction (targets[b.address & ((1<<TABLE_BITS)-1)]);
		    return vpc_predict();
        }
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
		if (bi.br_flags & BR_INDIRECT) {
			//targets[bi.address & ((1<<TABLE_BITS)-1)] = target;
		    vpc_update(u, target);
        }
	}

    void init_hashmap() 
    {
        for(int i = 0; i < MAX_ITER; ++i) {
            unsigned int r = rand();
            cout << "i: "<< i << " random number: " << r << "\n";
            hashmap[i] = r;

        }
        
    }

    void update_conditional_gshare(branch_update* u, bool taken, 
                        unsigned int vpca, unsigned int vghr,
                                    bool update_history=true) {
        
        unsigned int index = (vghr << (TABLE_BITS - HISTORY_LENGTH)) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
    
        unsigned char *c = &tab[index];
        if (taken) {
            if (*c < 3) (*c)++;
        } else {
            if (*c > 0) (*c)--;
        }

        if(update_history)
            update_conditional_history_gshare(taken);
    }

    void update_conditional_history_gshare(bool taken) {

        history <<= 1;
        history |= taken;
        history &= (1<<HISTORY_LENGTH)-1;
    }

    branch_update* vpc_predict(){
        unsigned int vpca = bi.address;
        unsigned int vghr = history;
if(DEBUG){
    cout<<  " predicted iteration initial value : " << u.prediction_iter << "\n";
}

        int i = 0;
        for(; i < MAX_ITER; ++i) {

            //compute the predicted target address
            unsigned int pred_target = targets[vpca & ((1<<TABLE_BITS)-1)];
            
            //calculate index at which the predicted direction to look for
            unsigned int index = (vghr << (TABLE_BITS - HISTORY_LENGTH)) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
			
            //see the MSB of the 2-bit counter
            bool pred_dir = tab[index] >> 1;

            if (pred_target && (pred_dir)) {
                //next_pc = pred_target;
                //
                //update the parameters, update this index to predicted_iteration
                u.prediction_iter = i;

                //update the target to predicted target
                u.target_prediction(pred_target);

                //update that one of the conditional branch is taken
                u.direction_prediction(true);
                break;
            }else if (!pred_target) {
                u.btb_miss_iter = i;
                u.direction_prediction(false);
                u.target_prediction(0);
                break;                
            }
            
            vpc_update_vpca_and_vghr(i, bi.address, vpca, vghr); 
        }
        if(i == MAX_ITER){
            u.btb_miss_iter = i-1;
            u.direction_prediction(false);
            u.target_prediction(0);
        }
if(DEBUG){
    cout << " Indirect branch pc address: "<< bi.address << 
            " predicted iteration : " << u.prediction_iter <<
            " btb miss iteration: " << u.btb_miss_iter <<
            " vghr : " << vghr << " vpca: "<< vpca <<
            " history : "<< history << "\n";
}
        return &u;
    }

    void vpc_update_vpca_and_vghr(int iter, unsigned int pc_addr,
                            unsigned int& vpca, unsigned int& vghr) {
        vpca = pc_addr ^ hashmap[iter];
        vghr <<= 1;
        vghr &= (1 << HISTORY_LENGTH)-1;
    }

    void vpc_update(branch_update* bu, unsigned int target)
    {
        //bool taken = u.direction_prediction();
        bool taken = u.target_prediction() & (u.target_prediction() == target);
        if(taken){//Algorithm 2
            unsigned int vpca = bi.address;
            unsigned int vghr = history;

            for(int i = 0; i <= u.prediction_iter; ++i){
                

                if(i == u.prediction_iter){
  
                    update_conditional_gshare(bu, true, vpca, vghr);
                   
                    //targets[vpca & ((1<<TABLE_BITS)-1)] = target;
                }else{
                    update_conditional_gshare(bu, false, vpca, vghr);    
                }
                
                vpc_update_vpca_and_vghr(i, bi.address, vpca, vghr);
            }
        
        }else {//Algorithm 3
            unsigned int vpca = bi.address;
            unsigned int vghr = history;

            bool found = false;
            int i = 0;
            for(; i < MAX_ITER; ++i){
                //compute the predicted target address
                unsigned int pred_target = targets[vpca & ((1<<TABLE_BITS)-1)];
            
                if(pred_target == target){
                    //update as taken
                    //update_conditional_gshare(bu, true, vpca, vghr, false); 

                    //update address
                    //targets[vpca & ((1<<TABLE_BITS)-1)] = target;
                    found = true;
                    break;
                }else if(pred_target){
                    //update as not taken
                    //update_conditional_gshare(bu, false, vpca, vghr, false);
                    
                }
                
                vpc_update_vpca_and_vghr(i, bi.address, vpca, vghr);
            }

            //not found case
            if(!found){

                vpca = bi.address;
                vghr = history;
                for(int i = 1; i <= u.btb_miss_iter; ++i){
                   
                    //if(i != u.btb_miss_iter)
                    update_conditional_gshare(bu, false, vpca, vghr);

                    vpc_update_vpca_and_vghr(i-1, bi.address, vpca, vghr); 

                }
                //vpc_update_vpca_and_vghr(u.btb_miss_iter, bi.address, vpca, vghr);
                //update address
                targets[vpca & ((1<<TABLE_BITS)-1)] = target;

                update_conditional_gshare(bu, true, vpca, vghr);    
            } else{
                
                unsigned int vpca = bi.address;
                unsigned int vghr = history;
                for(int j = 0; j < i; ++j){
                    update_conditional_gshare(bu, false, vpca, vghr);
                    vpc_update_vpca_and_vghr(j, bi.address, vpca, vghr);
                }

                update_conditional_gshare(bu, true, vpca, vghr);
                
            }

        }

if(DEBUG) {
    cout << "Updated History: "<< history << 
            " Any branch taken: "<< taken << 
            " target: " << target << "\n";


}
        u.prediction_iter = MAX_ITER;
        u.btb_miss_iter = MAX_ITER;
        u.target_prediction(0);
        //history <<= 1;
        //history |= taken;
        //history &= (1<<HISTORY_LENGTH)-1;
    }

};
