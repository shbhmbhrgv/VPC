// my_predictor.h
// This file contains a sample my_predictor class.
// It has a simple 32,768-entry gshare with a history length of 15 and a
// simple direct-mapped branch target buffer for indirect branch prediction.
#include <iostream>
using namespace std;

class my_update : public branch_update {
public:
	unsigned int index;
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
#define MAX_ITER 16
	my_update u;
	branch_info bi;
    int iter;
    int btb_miss_iter;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];
	unsigned int targets[1<<TABLE_BITS];
    unsigned int hashmap[MAX_ITER];

	my_predictor (void) : iter(0), btb_miss_iter(0), history(0) { 
		memset (tab, 0, sizeof (tab));
		memset (targets, 0, sizeof (targets));
        init_hashmap();
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		if (b.br_flags & BR_INDIRECT) {
			u.target_prediction (targets[b.address & ((1<<TABLE_BITS)-1)]);
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
			targets[bi.address & ((1<<TABLE_BITS)-1)] = target;
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

    void vpc_predict(){
        unsigned int vpca = bi.address;
        unsigned int vghr = history;

        for(int i = 0; i < MAX_ITER; ++i) {

            //compute the predicted target address
            unsigned int pred_target = targets[vpca & ((1<<TABLE_BITS)-1)];
            
            //calculate index at which the predicted direction to look for
            u.index = (vghr << (TABLE_BITS - HISTORY_LENGTH)) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
			
            //see the MSB of the 2-bit counter
            bool pred_dir = tab[u.index] >> 1;

            if (pred_target && (pred_dir == TAKEN)) {
                //next_pc = pred_target;
                iter = i;
                break;
            }else if (!pred_target) {
                btb_miss_iter = i;
                break;                
            }
            
            vpca = bi.address ^ hashmap[i];
            vghr <<= 1;
        }

    }


    void vpc_update(branch_update* u, bool taken, unsigned int target)
    {
        if(taken){
            unsigned int vpca = bi.address;
            unsigned int vghr = history;

            for(int i = 0; i <= iter; ++i){
                if(i == iter){
                    int index = (vghr << (TABLE_BITS - HISTORY_LENGTH)) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
                    unsigned char *c = &tab[index];
                    if (*c < 3) (*c)++;
                    history <<= 1;
                    history |= 1;
                    history &= (1<<HISTORY_LENGTH)-1;
                    targets[vpca & ((1<<TABLE_BITS)-1)] = vpca;
                }else{
                    int index = (vghr << (TABLE_BITS - HISTORY_LENGTH)) 
				                    ^ (vpca & ((1<<TABLE_BITS)-1));
                    unsigned char *c = &tab[index];
                    if (*c > 0) (*c)--;
                    
                    history <<= 1;
                    history |= 1;
                    history &= (1<<HISTORY_LENGTH)-1;
    
                }
                
                vpca = bi.address ^ hashmap[i];
                vghr <<= 1;

            }
        
        }

    }

    /*
     *Algorithm 2 VPC training algorithm when the branch target is correctly
predicted. Inputs: predicted_iter, PC, GHR
iter  1
V PCA   PC
V GHR   GHR
while (iter < predicted_iter) do
if (iter == predicted_iter) then
update_conditional_BP(V PCA, V GHR, TAKEN)
update_replacement_BTB(V PCA)
else
update_conditional_BP(V PCA, V GHR, NOT-TAKEN)
end if
V PCA  Hash(PC, iter)
V GHR  Left-Shift(V GHR)
iter++
end while
Algorithm 3 VPC training algorithm when the branch target is mispredicted.
Inputs: PC, GHR, CORRECT_TARGET
iter  1
V PCA   PC
V GHR   GHR
found_correct_target   FALSE
while ((iter  MAX_ITER) and (found_correct_target =
FALSE)) do
pred_target  access_BTB(V PCA)
if (pred_target = CORRECT_TARGET) then
update_conditional_BP(V PCA, V GHR, TAKEN)
update_replacement_BTB(V PCA)
found_correct_target   TRUE
else if (pred_target) then
update_conditional_BP(V PCA, V GHR, NOT-TAKEN)
end if
V PCA  Hash(PC, iter)
V GHR  Left-Shift(V GHR)
iter++
end while
/* no-target case */
if (found_correct_target = FALSE) then
V PCA   VPCA corresponding to the virtual branch with a BTB-Miss or
Least-frequently-used target among all virtual branches
V GHR   VGHR corresponding to the virtual branch with a BTB-Miss or
Least-frequently-used target among all virtual branches
insert_BTB(V PCA, CORRECT_TARGET)
update_conditional_BP(V PCA, V GHR, TAKEN)
end if
     *
     *
     *
     *
     *
     *
};
