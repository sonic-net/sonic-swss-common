use std::{
    env, fs,
    path::{Path, PathBuf},
};

fn main() {
    let header_paths = match env::var_os("SWSS_COMMON_REPO") {
        // if SWSS_COMMON_REPO is specified, we will build this library based on the files found in the repo
        Some(path_string) => {
            // Get full path of repo
            let path = Path::new(&path_string);
            let path = fs::canonicalize(path).expect("canonicalizing SWSS_COMMON_REPO path");
            assert!(path.exists(), "{path:?} doesn't exist");

            // Add libs path to linker search
            let libs_path = path.join("common/.libs").to_str().unwrap().to_string();
            println!("cargo:rustc-link-search=native={libs_path}");

            // include all of common/c-api/*.h
            find_headers_in_dir(path.join("common/c-api"))
        }

        // otherwise, we will assume the libswsscommon-dev package is installed and files
        // will be found at the locations defined in <sonic-swss-common repo>/debian/libswsscommon-dev.install
        None => {
            eprintln!("NOTE: If you are compiling without installing libswsscommon-dev, you must set SWSS_COMMON_REPO to the path of a built copy of the sonic-swss-common repo");
            find_headers_in_dir("/usr/include/swss/c-api")
        }
    };

    for h in &header_paths {
        println!("cargo:rerun-if-changed={h}");
    }

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("bindings.rs");
    bindgen::builder()
        .headers(&header_paths)
        .derive_partialeq(true)
        .generate()
        .unwrap()
        .write_to_file(out_path)
        .unwrap();

    println!("cargo:rustc-link-lib=dylib=swsscommon");
}

fn find_headers_in_dir(path: impl AsRef<Path>) -> Vec<String> {
    fs::read_dir(path)
        .unwrap()
        .map(|res_entry| res_entry.unwrap().path())
        .filter(|p| p.extension().is_some_and(|ext| ext == "h"))
        .map(|p| p.to_str().unwrap().to_string())
        .collect::<Vec<_>>()
}
