mod account;
mod error;

use account::{group, Account};
use clap::{ArgGroup, Args, Parser, Subcommand};
use eyre::Result;
use std::path::PathBuf;

#[derive(Parser)]
#[clap(version, author, about)]
struct AppArgs {
    #[clap(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Check duplicate passwords from command line
    Group(GroupArgs),
}

#[derive(Args)]
#[clap(group(
    ArgGroup::new("input")
        .required(true)
        .args(&["file", "account"]),
))]
struct GroupArgs {
    /// Account to check
    account: Vec<Account>,
    #[clap(short, long)]
    /// Load passwords from a file
    file: Option<PathBuf>,
}

fn main() -> Result<()> {
    let _ = color_eyre::install();
    // Check command line
    let args = AppArgs::parse();
    match args.command {
        Command::Group(GroupArgs {
            account: _,
            file: Some(filename),
        }) => {
            let accounts = Account::from_file(filename.as_path())?;
            let same_password_groups = group(&accounts);
            for entry in same_password_groups {
                println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
            }
        }
        Command::Group(args) => {
            let same_password_groups = group(&args.account);
            for entry in same_password_groups {
                println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
            }
        }
    }
    Ok(())
}
