.PHONY: web-build web-test web-lint web-lint-fix

web-build:
	python3 scripts/build_webui.py

web-test:
	python3 scripts/test_web.py

web-lint:
	python3 scripts/lint_web.py

web-lint-fix:
	python3 scripts/lint_web.py --fix
