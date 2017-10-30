// my_predictor.h
// This file contains a sample my_predictor class.
// It has a simple 32,768-entry gshare with a history length of 15 and a
// simple direct-mapped branch target buffer for indirect branch prediction.
#include <iostream>
#include <math.h>
using namespace std;

#define DEBUG 0
#define GSHARE 0
#define HISTORY 0
#define MAX_ITER 12
class my_update : public branch_update {
public:
	unsigned int index;
    int prediction_iter;
    int btb_miss_iter;
    int y_output;

    my_update (void) : branch_update(), 
                        index(0), prediction_iter(MAX_ITER), 
                        btb_miss_iter(MAX_ITER), y_output(0) {
    }
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH 61
#define TABLE_BITS	15
#define LEN_PERC_TABLE 4096
#define THRESHOLD 1.93*HISTORY_LENGTH + 14

	my_update u;
	branch_info bi;
	unsigned long long int history;
    
    // The array of perceptrons.
    int perceptron_table[LEN_PERC_TABLE][HISTORY_LENGTH+1];
    int bias[LEN_PERC_TABLE];

    unsigned char tab[1<<TABLE_BITS];
	unsigned int targets[1<<TABLE_BITS];
    unsigned int hashmap[MAX_ITER];
    unsigned int lfutable[1<<TABLE_BITS];
    
	my_predictor (void) : history(0){
        memset (perceptron_table, 0, perceptron_table[0][0]*LEN_PERC_TABLE*(HISTORY_LENGTH+1));
        memset (bias, 0, sizeof (bias));
		memset (tab, 0, sizeof (tab));
		memset (targets, 0, sizeof (targets));
        memset (lfutable, MAX_ITER, sizeof (lfutable));
        init_hashmap();
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
#if(GSHARE)
            //u.index = 
			//	  (history << (TABLE_BITS - HISTORY_LENGTH)) 
			//	^ (b.address & ((1<<TABLE_BITS)-1));

            u.index = 
				  (history ) 
				^ (b.address & ((1<<TABLE_BITS)-1));

			u.direction_prediction (tab[u.index] >> 1);
#else
            return perceptron_predict();
#endif
		} else {
			//u.direction_prediction (true);
		}
		if (b.br_flags & BR_INDIRECT) {
			//u.target_prediction (targets[b.address & ((1<<TABLE_BITS)-1)]);
		    return vpc_predict();
        }
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
#if(GSHARE)
            unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			//history &= (1<<HISTORY_LENGTH)-1;
            history &= (1<<15)-1;
#else
            update_conditional_perceptron(u, taken, bi.address, history);
#endif
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
            //cout << "i: "<< i << " random number: " << r << "\n";
            hashmap[i] = r;

        }
        
    }
#if 1
    void update_conditional_gshare(branch_update* u, bool taken, 
                        unsigned int vpca, unsigned long long int vghr,
                                    bool update_history=true) {
        
        unsigned int index = (vghr /*<< (TABLE_BITS - HISTORY_LENGTH)*/) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
    
        unsigned char *c = &tab[index];
        if (taken) {
            if (*c < 3) (*c)++;
        } else {
            if (*c > 0) (*c)--;
        }

        if(update_history)
            update_conditional_history(taken);
    }

    void update_conditional_history(bool taken) {

        history <<= 1;
        history |= taken;
        history &= (1ULL<<HISTORY_LENGTH)-1;
    }
#endif

    bool get_gshare_predicted_direction(unsigned long long int history, 
                                            unsigned pcaddr) 
    {
        unsigned int index = (history /*<< (TABLE_BITS - HISTORY_LENGTH)*/) 
				                    ^ (pcaddr & ((1<<TABLE_BITS)-1));
			
        //see the MSB of the 2-bit counter
        bool pred_dir = tab[index] >> 1;

        return pred_dir;
    }

    branch_update* perceptron_predict()
    {
        bool pred_dir;
        int y = 0;
        get_perceptron_predicted_dir_and_yout(history, bi.address, pred_dir, y);

        u.direction_prediction(pred_dir);        
        u.y_output = y;

        return &u;
    }

    void get_perceptron_predicted_dir_and_yout(unsigned long long int hist, 
                                                unsigned int pcaddr,
                                                bool& pred_dir, int& y)
    {
        //int perc_table_idx = pcaddr & ((1<<TABLE_BITS)-1);
        int perc_table_idx = pcaddr % LEN_PERC_TABLE;
        y = bias[perc_table_idx];
        for (int i = 0; i < HISTORY_LENGTH; ++i) {
            int history_table_entry = (hist & (1 << i)) ? 1 : -1;
            y += perceptron_table[perc_table_idx][i] * history_table_entry;
        }
        
        pred_dir = (y > 0) ? (true) : (false);

    }

    void update_conditional_perceptron(branch_update* u, bool taken, 
                                    unsigned int pcaddr,
                                    unsigned long long int hist) 
    {
        int y = ((my_update*)u)->y_output;
       
        update_conditional_perceptron(y, taken, pcaddr, hist);

    }

    void update_conditional_perceptron(int y, bool taken, unsigned int pcaddr,
                                            unsigned long long int hist)
    {
        int sign_y = (y > 0) ? 1 : -1;
        int sign_t = taken  ? 1 : -1;

        //int perc_table_idx = pcaddr & ((1<<TABLE_BITS)-1);
        int perc_table_idx = pcaddr % LEN_PERC_TABLE;

        int weight_threshold = (1 << static_cast<int>(log2(THRESHOLD))) -1;

        if ((sign_y != sign_t) || (abs(y) <= THRESHOLD)) {

            for (int i = 0; i < HISTORY_LENGTH+1; ++i) {

                int history_table_entry = (hist & (1 << i)) ? 1 : -1;
                if(abs(perceptron_table[perc_table_idx][i]) < weight_threshold)
                    perceptron_table[perc_table_idx][i] += sign_t * history_table_entry;
            }
        }
       
        if(abs(bias[perc_table_idx]) < weight_threshold)
            bias[perc_table_idx] += sign_t;

        update_conditional_history(taken); 
    }

#if 1

    branch_update* vpc_predict(){
        unsigned int vpca = bi.address;
        unsigned long long int vghr = history;
if(DEBUG){
    cout<<  " predicted iteration initial value : " << u.prediction_iter << "\n";
}

        int i = 0;
        for(; i < MAX_ITER; ++i) {

            //compute the predicted target address
            unsigned int pred_target = targets[vpca & ((1<<TABLE_BITS)-1)];
            bool pred_dir;
            int y_out = 0;
#if GSHARE 
            pred_dir = get_gshare_predicted_direction(vghr, vpca);
#else
            get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y_out);
#endif
            u.y_output = y_out;
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
                            unsigned int& vpca, unsigned long long int& vghr) {
        vpca = pc_addr ^ hashmap[iter];
        vghr <<= 1;
        vghr &= (1ULL << HISTORY_LENGTH)-1;
    }

    void vpc_update(branch_update* bu, unsigned int target)
    {
        bool taken = u.target_prediction() & (u.target_prediction() == target);
        if(taken){//Algorithm 2
            unsigned int vpca = bi.address;
            unsigned long long int vghr = history;

            for(int i = 0; i <= u.prediction_iter; ++i){
                

                if(i == u.prediction_iter){
#if (GSHARE) 
                    update_conditional_gshare(bu, true, vpca, vghr);
#else     
                    bool pred_dir;
                    int y = 0;
                    get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                    update_conditional_perceptron(y, true, vpca, vghr);
#endif
                }else{
#if (GSHARE)
                    update_conditional_gshare(bu, false, vpca, vghr);
#else
                    bool pred_dir;
                    int y = 0;
                    get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                    update_conditional_perceptron(y, false, vpca, vghr);
                    //update_conditional_perceptron(bu, false, vpca, vghr);
#endif
                }
                
                vpc_update_vpca_and_vghr(i, bi.address, vpca, vghr);
            }
        
        }else {//Algorithm 3
            unsigned int vpca = bi.address;
            unsigned long long int vghr = history;

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
                
                //if(u.btb_miss_iter == MAX_ITER-1){
                 //   cout << "lfu table case\n";
                //}

                for(int i = 1; i <= u.btb_miss_iter; ++i){
                   
#if (GSHARE)
                    update_conditional_gshare(bu, false, vpca, vghr);
#else
                    bool pred_dir;
                    int y = 0;
                    get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                    update_conditional_perceptron(y, false, vpca, vghr);
                    //update_conditional_perceptron(bu, false, vpca, vghr);
#endif
                    vpc_update_vpca_and_vghr(i-1, bi.address, vpca, vghr); 

                }
                //vpc_update_vpca_and_vghr(u.btb_miss_iter, bi.address, vpca, vghr);
                //update address
                targets[vpca & ((1<<TABLE_BITS)-1)] = target;
#if (GSHARE)
                update_conditional_gshare(bu, true, vpca, vghr);
#else
                bool pred_dir;
                int y = 0;
                get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                update_conditional_perceptron(y, true, vpca, vghr);
                //update_conditional_perceptron(bu, true, vpca, vghr);
#endif
            } else{
                
                unsigned int vpca = bi.address;
                unsigned long long int vghr = history;
                for(int j = 0; j < i; ++j){
#if (GSHARE)
                    update_conditional_gshare(bu, false, vpca, vghr);
#else
                    bool pred_dir;
                    int y = 0;
                    get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                    update_conditional_perceptron(y, false, vpca, vghr);
                    //update_conditional_perceptron(bu, false, vpca, vghr);
#endif
                    vpc_update_vpca_and_vghr(j, bi.address, vpca, vghr);
                }
#if (GSHARE)
                update_conditional_gshare(bu, true, vpca, vghr);
#else
                bool pred_dir;
                int y = 0;
                get_perceptron_predicted_dir_and_yout(vghr, vpca, pred_dir, y);
                update_conditional_perceptron(y, true, vpca, vghr);
                //update_conditional_perceptron(bu, true, vpca, vghr);
#endif
                
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
        u.direction_prediction(false);
        //history <<= 1;
        //history |= taken;
        //history &= (1<<HISTORY_LENGTH)-1;
    }
#endif

};
