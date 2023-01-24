
def write_to_csv_file(input, file_name):
    # Writing to file
    with open(file_name, "w") as file1:
        # Writing data to a file
        for i in input:
            file1.write(str(i) + ";")
        
def main():
    exit_flag = False
    addresses = []
    while not exit_flag:
        light_n = input("How many lights? \n")
        channel_n = input("How many channels? \n")
        first_position = input("First position of in the DMX universe \n")
        
        if(light_n != "" and channel_n != "" and first_position != ""):
            try:
                addresses = [int(first_position) +  i*int(channel_n) for i in range(int(light_n))]
                print("Addresses: ",addresses)
                exit =  input("Are these wanted addresses? (y/n)\n")
                
            except ValueError:
                print('You need to input numbers :D')
                exit = input('Do you want to continue? (y/n)\n')
        else: 
            print('You need to input something :D')
            exit = input('Do you want to continue? (y/n)\n')
        
        if exit != "n":
                exit_flag = True
    if len(addresses) > 0:
        file_name = "DMX_addresses.csv" 
        print("Writing to file: ", file_name)
        write_to_csv_file(addresses, file_name)

        

if __name__ == "__main__" :
        main()