# Reuse the application's compiled objects, replacing only its entry point.
# Run in <build>/application/app, alongside the generated Makefile:
# make -f Makefile -f <source>/tests/keyframe_navigation.mk navigation-regression \
#   NAVIGATION_SOURCE=<source>/tests/src/test_keyframe_navigation_integration.cpp

.PHONY: navigation-regression navigation-regression.o
navigation-regression: navigation-regression-bin

navigation-regression.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ "$(NAVIGATION_SOURCE)"

navigation-regression-bin: navigation-regression.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ navigation-regression.o $(filter-out main.o,$(OBJECTS)) $(LIBS)
