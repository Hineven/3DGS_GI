import os
import shutil

def copy_files(number):
    src = f"C:\\Users\\hineven\\PycharmProjects\\Relightable3DGaussian\\datasets\\neilfpp\\data_dtu\\DTU_scan{number}\\inputs\\images\\000000.png"
    dst = f"C:\\Users\\hineven\\Desktop\\scanned_pics\\{number}.png"
    
    # check if the source file exists
    if not os.path.exists(src):
        print(f"Source file {src} not found.")
        return

    try:
        shutil.copy(src, dst)
        print(f"File copied from {src} to {dst}")
    except FileNotFoundError:
        print(f"Source file {src} not found.")
    except Exception as e:
        print(f"Error occurred: {e}")

if __name__ == "__main__":
    # Example usage
    for i in range(1, 122):  # Adjust the range as needed
        copy_files(i)