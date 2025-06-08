# SaberEngine Pull Request Checklist

Please ensure the following guidelines are met before submitting your pull request. These help maintain consistency, clarity, and correctness across the codebase.

- [ ] **New files contain appropriate copyright/license headers**  
  Any newly created source or header files must include the standard copyright and license information.

- [ ] **Existing copyright/license headers have NOT been altered**  
  Do not modify headers in existing files unless there is a specific and documented reason to do so (e.g., legal update or ownership change).

- [ ] **Code style and conventions are preserved**  
  The code changes adhere to the existing project style, layout, naming conventions, indentation, comment tone, and maximum line width.

- [ ] **README.md is up to date**  
  If the pull request introduces changes that affect the usage, structure, or behavior of the project, the `README.md` must be updated accordingly to reflect those changes.

- [ ] **New small utility functions (e.g. getters/setters) are marked as `inline`**  
  Inline small, simple functions to avoid unnecessary function call overhead and improve compilation performance.

- [ ] **New standard library headers are added only to `pch.h`**  
  Do not include standard headers like `<vector>`, `<string>`, etc., directly in `.cpp` or `.h` files. Instead, add them to `pch.h` to reduce compile-time redundancy.

- [ ] **Prefer forward declarations over includes when possible**  
  Use forward declarations to reduce coupling and improve build times, especially in header files, unless a full type definition is explicitly required.

---

## Notes for Copilot and Reviewers

Please treat this checklist as binding guidance. Automated tools and AI coding assistants should adhere strictly to these items to ensure high-quality, consistent contributions.