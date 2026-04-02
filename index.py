import psutil
from datetime import datetime
import os

LOG_FILE = r"D:\\cpu_monitor_continuous.log"
# 限制日志文件最大大小 (例如 50MB)
MAX_LOG_SIZE = 50 * 1024 * 1024 
# 缓冲次数：在内存中积累多少次记录后再执行一次物理写入
BATCH_SIZE = 10 

def write_buffer_to_file(buffer):
    """负责将内存中的缓冲数据一次性写入磁盘"""
    # 检查日志文件大小，决定是追加还是覆盖
    if os.path.exists(LOG_FILE) and os.path.getsize(LOG_FILE) > MAX_LOG_SIZE:
        mode = 'w' # 覆盖重写
    else:
        mode = 'a' # 追加写入
        
    with open(LOG_FILE, mode, encoding='utf-8') as f:
        f.writelines(buffer)

def main():
    print(f"开始后台持续记录 Windows CPU 使用率...\n积累 {BATCH_SIZE} 次后批量写入。\n记录日志到: {LOG_FILE}")
    
    # 初始化预热
    psutil.cpu_percent(interval=None)
    for proc in psutil.process_iter():
        try:
            proc.cpu_percent(interval=None)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    log_buffer = [] # 内存缓冲区

    try:
        while True:
            # 阻塞 1 秒，计算总 CPU 利用率
            total_cpu = psutil.cpu_percent(interval=1)
            process_data = []

            # 遍历所有进程获取 CPU 数据
            for proc in psutil.process_iter(['pid', 'name', 'exe']):
                try:
                    p_cpu = proc.cpu_percent(interval=None) / psutil.cpu_count()
                    process_data.append({
                        'pid': proc.info['pid'],
                        'name': proc.info['name'],
                        'exe': proc.info['exe'] if proc.info['exe'] else '[无法获取路径 / 系统进程]',
                        'cpu': p_cpu
                    })
                except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                    pass

            # 直接排序取 Top 10
            top10 = sorted(process_data, key=lambda x: x['cpu'], reverse=True)[:10]
            
            # 组装单次的数据字符串 (此时不写文件，只拼接字符串)
            now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            record = []
            record.append("="*90 + "\n")
            record.append(f"时间: {now} | 当前系统总 CPU 占用率: {total_cpu}%\n")
            record.append(f"{'CPU(%)':<8} | {'PID':<8} | {'PROCESS_NAME':<20} | {'EXECUTION_PATH'}\n")
            record.append("-" * 90 + "\n")
            for p in top10:
                record.append(f"{p['cpu']:<8.2f} | {p['pid']:<8} | {p['name'][:20]:<20} | {p['exe']}\n")
            record.append("="*90 + "\n\n")

            # 将拼接好的单次记录塞入缓冲区
            log_buffer.append("".join(record))

            # 判断是否达到批量写入的阈值
            if len(log_buffer) >= BATCH_SIZE:
                write_buffer_to_file(log_buffer)
                log_buffer.clear() # 写入后清空缓冲区

    except KeyboardInterrupt:
        # 优雅退出：如果你在控制台按 Ctrl+C 停止，确保把还没凑够 10 次的剩余缓冲写进文件，防止丢失数据
        if log_buffer:
            write_buffer_to_file(log_buffer)
        print("\n监控已停止，剩余缓冲数据已落盘。")

if __name__ == "__main__":
    main()