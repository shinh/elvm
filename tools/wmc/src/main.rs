use anyhow::Result;
use thiserror::Error;

use std::path::Path;

extern crate getopts;

mod c;
mod compiler;
mod wmach_stream;

use compiler::Backend;

const OUTPUT_FILE: &'static str = "Program.h";

#[derive(Error, Debug)]
pub enum WmcError {
    #[error("Command-line help requested.")]
    Help,

    #[error("Missing CRT directory.")]
    MissingDirectory,

    #[error("Missing source code.")]
    MissingSource,
}

fn usage(opts: getopts::Options) -> Result<()> {
    let brief = format!("Usage: wmc [options]");
    eprintln!("{}", opts.usage(&brief));

    Err(WmcError::Help)?;

    Ok(())
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let mut opts = getopts::Options::new();
    opts.optopt(
        "o",
        "out",
        "directory to write Program.h, contains the C runtime",
        "CRT-DIR",
    );
    opts.optopt("s", "src", "source string to compile", "SRC-CODE");
    opts.optflag("h", "help", "print this help menu");

    let matches = opts.parse(&args[1..])?;
    if matches.opt_present("h") || !(matches.opt_present("o") || matches.opt_present("s")) {
        usage(opts)?;
    }

    let src_file = &matches.opt_str("s").ok_or(WmcError::MissingSource)?;
    let wm: c::Program = wmach_stream::Program::from_file(Path::new(src_file))?.compile()?;

    let crt_dir = &matches.opt_str("o").ok_or(WmcError::MissingDirectory)?;
    let crt_dir = Path::new(crt_dir);
    wm.save(&crt_dir.join(Path::new(OUTPUT_FILE)))?;

    Ok(())
}
