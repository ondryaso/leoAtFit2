.PHONE: kry clean pack

BUILD_DIR = cmake-build

kry:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make
	cp -f $(BUILD_DIR)/kry1 ./kry

clean:
	rm -rf $(BUILD_DIR)
	rm -f ./kry

pack: clean
	zip 221822.zip *.h *.cpp CMakeLists.txt Makefile