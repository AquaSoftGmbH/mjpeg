LIB_DIR = /usr/lib/site-python
BIN_DIR = /usr/local/bin
ETC_DIR = /etc

install:
	install -m 644 tvguide.py ${LIB_DIR}
	install -m 644 guide.py ${LIB_DIR}
	install -m 644 guidedbm.py ${LIB_DIR}
	install -m 644 events.py ${LIB_DIR}
	install -m 644 recorder.py ${LIB_DIR}
	install -m 644 table.py ${LIB_DIR}
	install -m 755 tvguide ${BIN_DIR}
	install -m 755 dtv ${BIN_DIR}
	install -m 755 recorder ${BIN_DIR}
	install -m 755 load_channel ${BIN_DIR}
	install -m 755 create_guide ${BIN_DIR}
	install -m 755 drop_events ${BIN_DIR}
	install -m 755 drop_listing ${BIN_DIR}
	install -m 644 dvcr ${ETC_DIR}
