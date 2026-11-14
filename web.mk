.PHONY: web-build web-test web-lint web-lint-fix web-preview

web-build:
	python3 scripts/build_webui.py

web-test:
	python3 scripts/test_web.py

web-lint:
	python3 scripts/lint_web.py

web-lint-fix:
	python3 scripts/lint_web.py --fix

# Dev server on :5173, proxied to the device at DEVICE_HOST.
web-preview:
	@test -n "$(DEVICE_HOST)" || { echo "set DEVICE_HOST=<device ip>"; exit 1; }
	docker run --rm --init $$([ -t 0 ] && echo -it) \
		--user $$(id -u):$$(id -g) -e HOME=/tmp -e npm_config_cache=/work/.npmcache \
		-e DEVICE_HOST=$(DEVICE_HOST) -p 5173:5173 \
		-v $(CURDIR)/web/firmware-client:/work/firmware-client \
		-v $(CURDIR)/web/client:/work/client \
		-v $(CURDIR)/.cache/node_modules:/work/firmware-client/node_modules \
		-v $(CURDIR)/.cache/npmcache:/work/.npmcache \
		-w /work/firmware-client node:22-alpine \
		sh -c 'npm install --no-fund --no-audit --prefer-offline && npx vite --port 5173 --strictPort'
