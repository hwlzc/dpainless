all: 
	##################################################
	###                Minisat                     ###
	##################################################
	+ $(MAKE) -C minisat

	##################################################
	###                Glucose                     ###
	##################################################
	+ $(MAKE) -C glucose

	##################################################
	###                Lingeling                   ###
	##################################################
	@if [ -f lingeling/makefile ]; then : ; \
      else cd lingeling && ./configure.sh; fi
	+ $(MAKE) liblgl.a -C lingeling

	##################################################
	###                MapleCOMSPS                 ###
	##################################################
	cd mapleCOMSPS && tar zxvf m4ri-20140914.tar.gz
	cd mapleCOMSPS/m4ri-20140914 && ./configure --enable-shared=no
	+ $(MAKE) -C mapleCOMSPS/m4ri-20140914
	+ $(MAKE) -C mapleCOMSPS

	##################################################
	###                PaInleSS                    ###
	##################################################
	+ $(MAKE) -C painless-src
	cd painless-src && mv painless ../painless

docs:
	rm -rf documents
	doxygen doxygen.config
	mkdir -p documents
	mv html latex documents

clean:
	##################################################
	###                Minisat                     ###
	##################################################
	+ $(MAKE) -C minisat clean

	##################################################
	###                Glucose                     ###
	##################################################
	+ $(MAKE) -C glucose clean

	##################################################
	###                Lingeling                   ###
	##################################################
	@if [ -f lingeling/makefile ]; then :; $(MAKE) -C lingeling clean; fi

	##################################################
	###                MapleCOMSPS                 ###
	##################################################
	rm -rf mapleCOMSPS/m4ri-20140914
	+ $(MAKE) -C mapleCOMSPS clean

	##################################################
	###                PaInleSS                    ###
	##################################################
	+ $(MAKE) clean -C painless-src
	rm -rf painless documents
