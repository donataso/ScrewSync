import js from "@eslint/js"
import stylistic from "@stylistic/eslint-plugin"
import reactHooks from "eslint-plugin-react-hooks"
import tseslint from "typescript-eslint"

export default tseslint.config(
  js.configs.recommended,
  ...tseslint.configs.recommended,
  {
    plugins: {
      "@stylistic": stylistic,
      "react-hooks": reactHooks,
    },
    rules: {
      curly: ["error", "all"],
      "@stylistic/brace-style": ["error", "1tbs", { allowSingleLine: false }],
      "no-empty": ["error", { allowEmptyCatch: true }],
      "@typescript-eslint/no-unused-vars": ["error", { ignoreRestSiblings: true }],
      "react-hooks/rules-of-hooks": "error",
      "react-hooks/exhaustive-deps": "error",
    },
  },
)
