# Number of cores to use when invoking parallelism
#ifndef CORES
CORES := 2
#endif

.DEFAULT_GOAL := blant
ifndef PAUSE   
	PAUSE := 1
endif
# Uncomment either of these to remove them (removing 7 implies removing 8)
EIGHT := 8
SEVEN := 7
ifdef NO8
    EIGHT := 
endif
ifdef NO7
    SEVEN :=
    EIGHT := # can't have 8 without 7
endif

HIGH_K_CMODEL_FLAGS :=
ifdef HIGH_K
    HIGH_K_CMODEL_FLAGS += -mcmodel=medium
endif

# to make the prediction version that agrees with the regression tests
ifdef PRED_REG
    PRED_REG_OPT := -DINTERNAL_DEG_WEIGHTS=0 -DDEG_ORDER_MUST_AGREE=1
endif

ifdef DEBUG
    ifdef PROFILE
	SPEED=-O0 -ggdb -pg
    else
	SPEED=-O0 -ggdb
    endif
else
    ifdef PROFILE
	SPEED=-O3 -pg
    else
	SPEED=-O3 #-DNDEBUG
    endif
endif

GCC=gcc$(GCC_VER)

UNAME=$(shell uname -a | awk '{if(/CYGWIN/){V="CYGWIN"}else if(/Darwin/){if(/arm64/)V="arm64";else V="Darwin"}else if(/Linux/){V="Linux"}}END{if(V){print V;exit}else{print "unknown OS" > "/dev/stderr"; exit 1}}')
STACKSIZE=$(shell ($(GCC) -v 2>/dev/null; uname -a) | awk '/CYGWIN/{print "-Wl,--stack,83886080"}/gcc-/{actualGCC=1}/Darwin/{print "-Wl,-stack_size -Wl,0x5000000"}')

CC=$(GCC) $(SPEED) $(NDEBUG) -Wno-misleading-indentation -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-variable -Wall -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wshadow $(PG)
CXX=g++$(GXX_VER) $(SPEED) $(NDEBUG)

# Include path: src/ contains all replacement headers for libwayne
BLANT_COMP=-I src $(SPEED) $(HIGH_K_CMODEL_FLAGS)
BLANT_LINK=-lm -lpthread $(STACKSIZE) $(SPEED) $(HIGH_K_CMODEL_FLAGS)
BLANT_BOTH=$(BLANT_COMP) $(BLANT_LINK)

# Name of BLANT source directory
SRCDIR = src

# All the headers
RAW_HEADERS = blant-fundamentals.h blant.h blant-output.h blant-predict.h blant-sampling.h blant-utils.h blant-window.h importance.h odv.h uthash.h blant-pthreads.h blant-fatal.h blant-utils-base.h blant-bitset.h blant-graph.h blant-graphlet.h blant-queue.h blant-multiset.h blant-heap.h blant-combin.h blant-stats.h blant-sim-anneal.h
BLANT_HEADERS = $(addprefix $(SRCDIR)/, $(RAW_HEADERS))

# Put all c files in SRCDIR below.
BLANT_SRCS = blant.c \
			 blant-window.c \
			 blant-output.c \
			 blant-utils.c \
			 blant-sampling.c \
			 blant-predict.o \
			 blant-synth-graph.c \
			 importance.c \
			 odv.c \
			 blant-pthreads.c

OBJDIR = _objs
BLANT_CANON_DIR = canon_maps
OBJS = $(addprefix $(OBJDIR)/, $(BLANT_SRCS:.c=.o))

ifneq ("$(wildcard $(SRCDIR)/EdgePredict/blant-predict.c)","")
    BLANT_PREDICT_SRC = $(SRCDIR)/EdgePredict/blant-predict.c
else
    BLANT_PREDICT_SRC = $(SRCDIR)/blant-predict.c
endif


### Generated File Lists ###

K := 3 4 5 6 $(SEVEN) $(EIGHT)
alpha_sampling_methods := NBE EBE MCMC
alpha_txts := $(foreach method,$(alpha_sampling_methods),$(BLANT_CANON_DIR)/alpha_list_$(method))
canon_txt := $(BLANT_CANON_DIR)/canon_map $(BLANT_CANON_DIR)/canon_list $(BLANT_CANON_DIR)/canon-ordinal-to-signature $(BLANT_CANON_DIR)/orbit_map $(alpha_txts)
canon_bin := $(BLANT_CANON_DIR)/canon_map $(BLANT_CANON_DIR)/perm_map

canon_all := $(foreach k, $(K), $(addsuffix $(k).txt, $(canon_txt)) $(addsuffix $(k).bin, $(canon_bin)))
subcanon_txts := $(if $(EIGHT),$(BLANT_CANON_DIR)/subcanon_map8-7.txt) $(if $(SEVEN),$(BLANT_CANON_DIR)/subcanon_map7-6.txt) $(BLANT_CANON_DIR)/subcanon_map6-5.txt $(BLANT_CANON_DIR)/subcanon_map5-4.txt $(BLANT_CANON_DIR)/subcanon_map4-3.txt
ifneq ("$(wildcard orca_jesse_blant_table)","")
magic_table_txts := $(foreach k,$(K), orca_jesse_blant_table/UpperToLower$(k).txt)
else
magic_table_txts :=
endif

base: ./.notpristine show-gcc-ver $(canon_all) magic_table blant test_all

.PHONY: k3 k4 k5 k6 k7 k8

k3: $(addsuffix 3.txt, $(canon_txt)) $(addsuffix 3.bin, $(canon_bin))
k4: $(addsuffix 4.txt, $(canon_txt)) $(addsuffix 4.bin, $(canon_bin))
k5: $(addsuffix 5.txt, $(canon_txt)) $(addsuffix 5.bin, $(canon_bin))
k6: $(addsuffix 6.txt, $(canon_txt)) $(addsuffix 6.bin, $(canon_bin))
k7: $(addsuffix 7.txt, $(canon_txt)) $(addsuffix 7.bin, $(canon_bin))
k8: $(addsuffix 8.txt, $(canon_txt)) $(addsuffix 8.bin, $(canon_bin))

show-gcc-ver:
	$(GCC) -v

./.notpristine:
	@echo '************ READ THIS. REALLY. WE MEAN IT. READ IT AT LEAST ONCE **************'
	@echo "BLANT can sample graphlets of up to k=8 nodes."
	@echo "The fastest way to get started is: PAUSE=0 NO8=1 make base"
	@echo '****************************************'
	sleep $(PAUSE)
	@touch .notpristine

most: base Draw sub$(BLANT_CANON_DIR)

test_all: $(BLANT_CANON_DIR)/test_index_mode $(BLANT_CANON_DIR)/check_maps test_fast

all: most test_all

$(BLANT_CANON_DIR): base $(canon_all) sub$(BLANT_CANON_DIR)

.PHONY: all test_all most pristine clean_$(BLANT_CANON_DIR)

### Executables ###

# Build mt19937 stub (USE_MarsenneTwister is off, but blant needs C++ link)
$(OBJDIR)/mt19937.o: $(SRCDIR)/mt19937_stub.cpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++11 -c $< -o $@

fast-canon-map: $(SRCDIR)/fast-canon-map.c | $(SRCDIR)/blant.h $(OBJDIR)/libblant.o
	$(CC) '-std=c99' -O3 -o $@ $(OBJDIR)/libblant.o $(SRCDIR)/fast-canon-map.c $(BLANT_BOTH)

slow-canon-maps: $(SRCDIR)/slow-canon-maps.c | $(SRCDIR)/blant.h $(OBJDIR)/libblant.o
	$(CC) -o $@ $(OBJDIR)/libblant.o $(SRCDIR)/slow-canon-maps.c $(BLANT_BOTH)

make-orbit-maps: $(SRCDIR)/make-orbit-maps.c | $(SRCDIR)/blant.h $(OBJDIR)/libblant.o
	$(CC) -o $@ $(OBJDIR)/libblant.o $(SRCDIR)/make-orbit-maps.c $(BLANT_BOTH)

blant: $(OBJS) $(OBJDIR)/libblant.o $(OBJDIR)/mt19937.o
	$(CXX) -o $@ $(OBJDIR)/libblant.o $(OBJS) $(OBJDIR)/mt19937.o $(BLANT_LINK)
	./canon-upper.sh

BLANT_FAST_FLAGS=-DPARANOID_ASSERTS=0 -DNDEBUG -march=native

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(BLANT_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(BLANT_FAST_FLAGS) -c -o $@ $< $(BLANT_COMP)

synthetic: $(SRCDIR)/synthetic.c $(SRCDIR)/syntheticDS.h $(SRCDIR)/syntheticDS.c | $(OBJDIR)/libblant.o
	$(CC) -c $(SRCDIR)/syntheticDS.c $(SRCDIR)/synthetic.c $(BLANT_COMP)
	$(CXX) -o $@ syntheticDS.o $(OBJDIR)/libblant.o synthetic.o $(BLANT_LINK)

makeEHD: $(OBJDIR)/makeEHD.o
	$(CXX) -o $@ $(OBJDIR)/libblant.o $(OBJDIR)/makeEHD.o $(BLANT_LINK)

compute-alphas-NBE: $(SRCDIR)/compute-alphas-NBE.c | $(OBJDIR)/libblant.o
	$(CC) -Wall -O3 -o $@ $(SRCDIR)/compute-alphas-NBE.c $(OBJDIR)/libblant.o $(BLANT_BOTH)

compute-alphas-EBE: $(SRCDIR)/compute-alphas-EBE.c | $(OBJDIR)/libblant.o
	$(CC) -Wall -O3 -o $@ $(SRCDIR)/compute-alphas-EBE.c $(OBJDIR)/libblant.o $(BLANT_BOTH)

compute-alphas-MCMC-slow: $(SRCDIR)/compute-alphas-MCMC-slow.c | $(OBJDIR)/libblant.o
	$(CC) -Wall -O3 -o $@ $(SRCDIR)/compute-alphas-MCMC-slow.c $(OBJDIR)/libblant.o $(BLANT_BOTH)

compute-alphas-MCMC: $(SRCDIR)/compute-alphas-MCMC.c | $(OBJDIR)/libblant.o
	$(CC) -Wall -O3 -o $@ $(SRCDIR)/compute-alphas-MCMC.c $(OBJDIR)/libblant.o $(BLANT_BOTH)

Draw: Draw/graphette2dot

Draw/graphette2dot: Draw/DrawGraphette.cpp Draw/Graphette.cpp Draw/Graphette.h Draw/graphette2dotutils.cpp Draw/graphette2dotutils.h | $(SRCDIR)/blant.h $(OBJDIR)/libblant.o
	$(CXX) Draw/DrawGraphette.cpp Draw/graphette2dotutils.cpp Draw/Graphette.cpp $(OBJDIR)/libblant.o -o $@ -std=gnu++11 $(BLANT_BOTH)

make-subcanon-maps: $(SRCDIR)/make-subcanon-maps.c | $(OBJDIR)/libblant.o
	$(CC) -Wall -o $@ $(SRCDIR)/make-subcanon-maps.c $(OBJDIR)/libblant.o $(BLANT_BOTH)

make-orca-jesse-blant-table: $(SRCDIR)/blant-fundamentals.h $(SRCDIR)/magictable.cpp | $(OBJDIR)/libblant.o
	$(CXX) -Wall -o $@ $(SRCDIR)/magictable.cpp $(OBJDIR)/libblant.o -std=gnu++11 $(BLANT_BOTH)

cluster-similarity-graph: src/cluster-similarity-graph.c
	$(CC) $(BLANT_COMP) $(SPEED) -Wall -o $@ $(SRCDIR)/cluster-similarity-graph.c

$(OBJDIR)/blant-predict.o: $(BLANT_PREDICT_SRC)
	if [ -f $(SRCDIR)/EdgePredict/Makefile ]; then (CC="$(CC) $(PRED_REG_OPT) $(BLANT_COMP)"; export CC; OBJDIR="$(OBJDIR)"; export OBJDIR; cd $(SRCDIR)/EdgePredict && $(MAKE)); else $(CC) $(PRED_REG_OPT) -c -o $@ $(SRCDIR)/blant-predict.c $(BLANT_BOTH); fi

### Object Files/Prereqs ###

$(OBJDIR)/convert.o: $(SRCDIR)/convert.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(SRCDIR)/convert.cpp -o $@ -std=gnu++11

$(OBJDIR)/libblant.o: $(SRCDIR)/libblant.c
	@mkdir -p $(dir $@)
	$(CC) $(BLANT_FAST_FLAGS) -c $(SRCDIR)/libblant.c -o $@ $(BLANT_COMP)

$(OBJDIR)/makeEHD.o: $(SRCDIR)/makeEHD.c | $(OBJDIR)/libblant.o
	@mkdir -p $(dir $@)
	$(CC) -c $(SRCDIR)/makeEHD.c -o $@ $(BLANT_COMP)

### Generated File Recipes

$(BLANT_CANON_DIR)/canon_map%.txt $(BLANT_CANON_DIR)/canon_list%.txt $(BLANT_CANON_DIR)/canon-ordinal-to-signature%.txt: fast-canon-map
	mkdir -p $(BLANT_CANON_DIR)
	[ $* -eq 8 -a '(' -f $(BLANT_CANON_DIR)/canon_map$*.txt -o -f $(BLANT_CANON_DIR)/canon_map$*.txt.gz ')' ] || ./fast-canon-map $* | tee $(BLANT_CANON_DIR)/canon_map$*.txt | awk -F '	' 'BEGIN{n=0}!seen[$$1]{seen[$$1]=$$0;map[n++]=$$1}END{print n;for(i=0;i<n;i++)print seen[map[i]]}' | cut -f1,3- | tee $(BLANT_CANON_DIR)/canon_list$*.txt | awk 'NR>1{print NR-2, $$1}' > $(BLANT_CANON_DIR)/canon-ordinal-to-signature$*.txt

$(BLANT_CANON_DIR)/alpha_list_NBE%.txt: compute-alphas-NBE $(BLANT_CANON_DIR)/canon_list%.txt
	./compute-alphas-NBE $* > $@

$(BLANT_CANON_DIR)/alpha_list_EBE%.txt: compute-alphas-EBE $(BLANT_CANON_DIR)/canon_list%.txt
	./compute-alphas-EBE $* > $@

$(BLANT_CANON_DIR)/alpha_list_MCMC%.txt: compute-alphas-MCMC $(BLANT_CANON_DIR)/canon_list%.txt
	./compute-alphas-MCMC $* > $(BLANT_CANON_DIR)/alpha_list_MCMC$*.txt;

$(BLANT_CANON_DIR)/orbit_map%.txt: make-orbit-maps $(BLANT_CANON_DIR)/canon_list%.txt
	./make-orbit-maps $* > $(BLANT_CANON_DIR)/orbit_map$*.txt

$(BLANT_CANON_DIR)/canon_map%.bin $(BLANT_CANON_DIR)/perm_map%.bin: $(SRCDIR)/create-bin-data.c $(BLANT_CANON_DIR)/canon_list%.txt $(BLANT_CANON_DIR)/canon_map%.txt
	$(CC) '-std=c99' "-Dkk=$*" "-DkString=\"$*\"" -o create-bin-data$* $(SRCDIR)/libblant.c $(SRCDIR)/create-bin-data.c $(BLANT_BOTH)
	[ -f $(BLANT_CANON_DIR)/canon_map$*.bin -a -f $(BLANT_CANON_DIR)/perm_map$*.bin ] || ./create-bin-data$*

$(BLANT_CANON_DIR)/EdgeHammingDistance%.txt: makeEHD | $(BLANT_CANON_DIR)/canon_list%.txt $(BLANT_CANON_DIR)/canon_map%.bin
	@if [ ! -f $(BLANT_CANON_DIR).correct/EdgeHammingDistance$*.txt.xz ]; then ./makeEHD $* > $@; cmp $(BLANT_CANON_DIR).correct/EdgeHammingDistance$*.txt $@; else echo "EdgeHammingDistance8.txt takes weeks to generate; uncompressing instead"; unxz < $(BLANT_CANON_DIR).correct/EdgeHammingDistance$*.txt.xz > $@ && touch $@; fi

.INTERMEDIATE: .created-subcanon-maps
sub$(BLANT_CANON_DIR): $(subcanon_txts) ;
$(subcanon_txts): .created-subcanon-maps
.created-subcanon-maps: make-subcanon-maps | $(canon_all)
	for k in $(K); do if [ $$k -gt 3 ]; then ./make-subcanon-maps $$k > $(BLANT_CANON_DIR)/subcanon_map$$k-$$(($$k-1)).txt; fi; done

magic_table: $(magic_table_txts) ;
$(magic_table_txts): make-orca-jesse-blant-table | $(canon_all)
	./make-orca-jesse-blant-table $(if $(EIGHT),8,$(if $(SEVEN),7,6))

### Testing ###

blant-sanity: $(SRCDIR)/blant-sanity.c
	$(CC) -o $@ $(SRCDIR)/blant-sanity.c $(BLANT_BOTH)

test_stamp: blant blant-sanity $(canon_all) $(subcanon_txts)
	@echo Touching test_stamp
	@if [ -n "$?" ] && { [ "$$(echo "$?" | wc -w)" -ne 1 ] || [ "$?" != "$(BLANT_CANON_DIR)/canon_map8.txt" ]; }; then \
		touch test_stamp; \
	fi

test_fast: blant blant-sanity
	for k in $(K); do if [ -f $(BLANT_CANON_DIR)/canon_map$$k.bin ]; then echo FAST basic sanity for ONLY MCMC with k=$$k; ./blant -q -s MCMC -mi -n 100000 -k $$k networks/syeast.el | sort -n | ./blant-sanity $$k 100000 networks/syeast.el; fi; done

$(BLANT_CANON_DIR)/test_index_mode: test_stamp
	touch $(BLANT_CANON_DIR)/test_index_mode
	for S in NBE MCMC EBE; do for k in $(K); do if [ -f $(BLANT_CANON_DIR)/canon_map$$k.bin ]; then echo basic sanity check sampling method $$S indexing k=$$k; ./blant -q -s $$S -mi -n 100000 -k $$k networks/syeast.el | sort -n | ./blant-sanity $$k 100000 networks/syeast.el; fi; done; done

$(BLANT_CANON_DIR)/check_maps: test_stamp
	touch $(BLANT_CANON_DIR)/check_maps
	ls $(BLANT_CANON_DIR).correct/ | egrep -v 'canon_list2|$(if $(SEVEN),,7|)$(if $(EIGHT),,8|)README|\.[gx]z|EdgeHamming' | awk '{printf "cmp $(BLANT_CANON_DIR).correct/%s $(BLANT_CANON_DIR)/%s\n",$$1,$$1}' | sh

.PHONY: test_fast

### Cleaning ###

clean:
	@/bin/rm -f *.[oa] blant create-bin-data3 create-bin-data4 create-bin-data5 create-bin-data6 create-bin-data7 create-bin-data8 canon-sift fast-canon-map make-orbit-maps compute-alphas-MCMC-slow compute-alphas-MCMC compute-alphas-NBE compute-alphas-EBE make-orca-jesse-blant-table Draw/graphette2dot blant-sanity make-subcanon-maps test_stamp $(BLANT_CANON_DIR)/check_maps $(BLANT_CANON_DIR)/test_index_mode
	@/bin/rm -rf $(OBJDIR)/*

realclean:
	echo "'realclean' is now called 'pristine'; try again"
	false

pristine: clean clean_$(BLANT_CANON_DIR)
	@/bin/rm -f $(BLANT_CANON_DIR)/* .notpristine .firsttime

clean_$(BLANT_CANON_DIR):
	@/bin/rm -f $(BLANT_CANON_DIR)/*[3-7].*
	@/bin/rm -f orca_jesse_blant_table/UpperToLower*.txt
