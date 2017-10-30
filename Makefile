CXX		=	g++ -std=c++11
CXXFLAGS	=	-g -O3 -Wall

all:		predict

predict:	predict.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict predict.cc trace.cc

clean:
		rm -f predict
